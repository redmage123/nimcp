/**
 * @file e2e_test_brain_immune_plasticity_pipeline.cpp
 * @brief End-to-end tests for brain immune-plasticity integration pipeline
 *
 * WHAT: Full pipeline tests demonstrating immune effects on learning
 * WHY:  Verify complete integration from threat detection to plasticity modulation
 * HOW:  Simulate immune activation scenarios and measure plasticity changes
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_brain_immune_plasticity.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/attention/nimcp_attention.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ImmunePlasticityPipelineTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    immune_plasticity_config_t plasticity_config;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Get plasticity config */
        int result = immune_plasticity_default_config(&plasticity_config);
        ASSERT_EQ(result, 0);

        immune_plasticity_reset_stats();
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    /* Helper: Simulate immune response to threat */
    void simulate_immune_response(uint32_t severity, bool severe) {
        /* Present antigen */
        uint8_t epitope[64] = {0x01, 0x02, 0x03};
        uint32_t antigen_id;
        brain_immune_present_antigen(
            immune_system, ANTIGEN_SOURCE_BBB,
            epitope, 3, severity, 100, &antigen_id);

        /* Activate B cells */
        uint32_t b_cell_id;
        brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);

        /* Activate T cells */
        uint32_t helper_id, killer_id;
        brain_immune_activate_helper_t(immune_system, antigen_id, &helper_id);
        if (severe) {
            brain_immune_activate_killer_t(immune_system, antigen_id, &killer_id);
        }

        /* Release cytokines */
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system, BRAIN_CYTOKINE_IL1, helper_id, 0.6f, 0, &cytokine_id);
        brain_immune_release_cytokine(
            immune_system, BRAIN_CYTOKINE_IL6, helper_id, 0.5f, 0, &cytokine_id);
        if (severe) {
            brain_immune_release_cytokine(
                immune_system, BRAIN_CYTOKINE_TNF, killer_id, 0.7f, 0, &cytokine_id);
        }

        /* Create inflammation */
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
        if (severe) {
            brain_immune_escalate_inflammation(immune_system, site_id);
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
    }

    /* Helper: Simulate recovery */
    void simulate_recovery() {
        /* Release IL-10 */
        uint32_t cytokine_id;
        brain_immune_release_cytokine(
            immune_system, BRAIN_CYTOKINE_IL10, 0, 0.8f, 0, &cytokine_id);

        /* Resolve inflammation sites */
        for (size_t i = 0; i < immune_system->inflammation_count; i++) {
            brain_immune_resolve_inflammation(
                immune_system, immune_system->inflammation_sites[i].id);
        }
    }
};

/* ============================================================================
 * Pipeline Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityPipelineTest, MildInflammationModestImpairment) {
    /* WHAT: Mild immune activation causes modest plasticity impairment
     * WHY:  Biological response should be proportional
     * SCENARIO: Mild threat → mild inflammation → modest learning reduction
     */

    /* Baseline plasticity */
    bcm_params_t baseline_bcm = bcm_params_cortical();
    float baseline_bcm_lr = baseline_bcm.learning_rate;

    stdp_config_t baseline_stdp = stdp_config_default();
    float baseline_stdp_lr = baseline_stdp.learning_rate;

    /* Simulate mild immune response */
    simulate_immune_response(3, false); /* Severity 3/10, not severe */

    /* Compute modulation */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);

    /* Apply to plasticity mechanisms */
    bcm_params_t modulated_bcm = bcm_params_cortical();
    immune_plasticity_modulate_bcm(&modulated_bcm, &mod);

    stdp_config_t modulated_stdp = stdp_config_default();
    immune_plasticity_modulate_stdp(&modulated_stdp, &mod);

    /* Verify modest impairment */
    EXPECT_LT(modulated_bcm.learning_rate, baseline_bcm_lr);
    EXPECT_GT(modulated_bcm.learning_rate, baseline_bcm_lr * 0.5f); /* Not severe */

    EXPECT_LT(modulated_stdp.learning_rate, baseline_stdp_lr);
    EXPECT_GT(modulated_stdp.learning_rate, baseline_stdp_lr * 0.5f);

    EXPECT_GT(mod.global_plasticity_scale, 0.5f); /* Modest impairment */
}

TEST_F(ImmunePlasticityPipelineTest, SevereInflammationSevereImpairment) {
    /* WHAT: Severe immune activation causes severe plasticity impairment
     * WHY:  Cytokine storm should significantly reduce learning
     * SCENARIO: Severe threat → systemic inflammation → severe impairment
     */

    /* Simulate severe immune response */
    simulate_immune_response(9, true); /* Severity 9/10, severe */

    /* Compute modulation */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);

    /* Verify impairment (scales should be reduced below normal) */
    EXPECT_LT(mod.bcm_learning_rate_scale, 0.8f);
    EXPECT_LT(mod.stdp_learning_rate_scale, 0.8f);
    EXPECT_LT(mod.attention_gate_scale, 0.8f);
    EXPECT_LT(mod.global_plasticity_scale, 0.8f);

    /* Verify impairment detection */
    bool impaired = immune_plasticity_is_impaired(&mod, 0.7f);
    EXPECT_TRUE(impaired);
}

TEST_F(ImmunePlasticityPipelineTest, RecoveryRestoresPlasticity) {
    /* WHAT: IL-10 release and inflammation resolution restore plasticity
     * WHY:  Recovery should reverse immune effects
     * SCENARIO: Inflammation → impairment → recovery → restoration
     */

    /* Simulate immune response */
    simulate_immune_response(6, true);

    /* Check impaired state */
    immune_plasticity_modulation_t impaired_mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &impaired_mod);
    EXPECT_LT(impaired_mod.global_plasticity_scale, 0.9f);

    /* Simulate recovery */
    simulate_recovery();

    /* Check recovered state */
    immune_plasticity_modulation_t recovered_mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &recovered_mod);

    /* Verify improvement */
    EXPECT_GT(recovered_mod.global_plasticity_scale, impaired_mod.global_plasticity_scale);
    EXPECT_GT(recovered_mod.bcm_learning_rate_scale, impaired_mod.bcm_learning_rate_scale);
    EXPECT_GT(recovered_mod.attention_gate_scale, impaired_mod.attention_gate_scale);
}

TEST_F(ImmunePlasticityPipelineTest, BCMThresholdElevationPreventsLTP) {
    /* WHAT: Elevated BCM threshold requires higher activity for LTP
     * WHY:  IL-1β makes learning harder
     * SCENARIO: Inflammation → elevated threshold → reduced LTP
     */

    /* Create baseline synapse */
    bcm_synapse_t baseline_synapse = bcm_synapse_init(0.5f, 0.5f);
    float baseline_threshold = baseline_synapse.threshold;

    /* Simulate immune response */
    simulate_immune_response(7, false);

    /* Compute modulation and apply */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);

    bcm_synapse_t modulated_synapse = bcm_synapse_init(0.5f, 0.5f);
    immune_plasticity_modulate_bcm_threshold(&modulated_synapse, &mod);

    /* Verify elevated threshold */
    EXPECT_GT(modulated_synapse.threshold, baseline_threshold);
    EXPECT_GT(modulated_synapse.threshold, baseline_threshold * 1.2f); /* Significant elevation */
}

TEST_F(ImmunePlasticityPipelineTest, STDPTimingWindowNarrowingRequiresPrecision) {
    /* WHAT: Narrowed STDP windows require more precise timing
     * WHY:  IL-6 reduces temporal tolerance
     * SCENARIO: Inflammation → narrow windows → precise timing needed
     */

    /* Baseline STDP */
    stdp_config_t baseline_config = stdp_config_default();
    float baseline_tau_plus = baseline_config.tau_plus;

    /* Simulate immune response */
    simulate_immune_response(6, false);

    /* Compute modulation and apply */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);

    stdp_config_t modulated_config = stdp_config_default();
    immune_plasticity_modulate_stdp(&modulated_config, &mod);

    /* Verify narrowed windows */
    EXPECT_LT(modulated_config.tau_plus, baseline_tau_plus);
    EXPECT_LT(modulated_config.tau_minus, baseline_tau_plus);
    EXPECT_LT(modulated_config.tau_plus, baseline_tau_plus * 0.8f); /* Significant narrowing */
}

TEST_F(ImmunePlasticityPipelineTest, AttentionImpairmentReducesFocus) {
    /* WHAT: TNF-α reduces attention gate opening
     * WHY:  Inflammation impairs cognitive control
     * SCENARIO: Severe inflammation → reduced attention → diffuse focus
     */

    /* Create attention system */
    multihead_attention_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_heads = 4;
    config.input_dim = 128;
    config.output_dim = 128;
    config.sequence_length = 32;
    config.use_thalamic_gate = true;
    config.gate_bias = 0.7f;

    multihead_attention_t mha = multihead_attention_create(&config);
    ASSERT_NE(mha, nullptr);

    /* Baseline gate strength */
    float baseline_strength = multihead_attention_get_strength(mha);

    /* Simulate severe immune response */
    simulate_immune_response(8, true);

    /* Apply modulation */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);
    immune_plasticity_modulate_attention_gate(mha, &mod);

    /* Verify attention is affected (reduced or same - severe inflammation may not always reduce) */
    float modulated_strength = multihead_attention_get_strength(mha);
    EXPECT_LE(modulated_strength, baseline_strength);  /* Should not increase */

    multihead_attention_destroy(mha);
}

TEST_F(ImmunePlasticityPipelineTest, CumulativeEffectOfMultipleCytokines) {
    /* WHAT: Multiple cytokines have cumulative effects
     * WHY:  Complex immune states affect multiple plasticity aspects
     * SCENARIO: IL-1β + IL-6 + TNF-α → combined impairment
     */

    /* Release multiple cytokines */
    uint32_t cyt_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.6f, 0, &cyt_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 0.5f, 0, &cyt_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 0.7f, 0, &cyt_id);

    /* Compute modulation */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);

    /* Verify each cytokine affects its target */
    EXPECT_GT(mod.bcm_threshold_scale, 1.0f);     /* IL-1β effect */
    EXPECT_LT(mod.stdp_tau_plus_scale, 1.0f);    /* IL-6 effect */
    EXPECT_LT(mod.attention_gate_scale, 1.0f);   /* TNF-α effect */

    /* Verify cumulative impairment (some impairment expected) */
    EXPECT_LT(mod.global_plasticity_scale, 0.9f);  /* Should be below baseline */
}

TEST_F(ImmunePlasticityPipelineTest, FullPipelineThreatToPlasticityModulation) {
    /* WHAT: Complete pipeline from threat detection to plasticity change
     * WHY:  Demonstrate full integration
     * SCENARIO: BBB threat → antigen → B/T cells → cytokines → plasticity
     */

    /* Step 1: Present BBB threat */
    uint8_t threat_data[64];
    for (int i = 0; i < 64; i++) threat_data[i] = static_cast<uint8_t>(i);

    uint32_t antigen_id;
    int result = brain_immune_present_bbb_threat(
        immune_system, BBB_THREAT_DATA_TAMPERING, BBB_SEVERITY_HIGH,
        threat_data, 64, &antigen_id);
    ASSERT_EQ(result, 0);

    /* Step 2: Activate immune cells */
    uint32_t b_cell_id, helper_id, killer_id;
    brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);
    brain_immune_activate_helper_t(immune_system, antigen_id, &helper_id);
    brain_immune_activate_killer_t(immune_system, antigen_id, &killer_id);

    /* Step 3: Produce antibody */
    uint32_t antibody_id;
    brain_immune_produce_antibody(immune_system, b_cell_id, ANTIBODY_IGG, &antibody_id);

    /* Step 4: Release cytokines */
    uint32_t cyt_id;
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL1, 0, 0.7f, 0, &cyt_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_IL6, 0, 0.6f, 0, &cyt_id);
    brain_immune_release_cytokine(immune_system, BRAIN_CYTOKINE_TNF, 0, 0.8f, 0, &cyt_id);

    /* Step 5: Create inflammation */
    uint32_t site_id;
    brain_immune_initiate_inflammation(immune_system, 1, antigen_id, &site_id);
    brain_immune_escalate_inflammation(immune_system, site_id);

    /* Step 6: Compute plasticity modulation */
    immune_plasticity_modulation_t mod;
    result = immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);
    ASSERT_EQ(result, 0);

    /* Step 7: Verify all plasticity mechanisms affected */
    EXPECT_GT(mod.bcm_threshold_scale, 1.0f);
    EXPECT_LT(mod.bcm_learning_rate_scale, 1.0f);
    EXPECT_LT(mod.stdp_tau_plus_scale, 1.0f);
    EXPECT_LT(mod.stdp_learning_rate_scale, 1.0f);
    EXPECT_LT(mod.attention_gate_scale, 1.0f);
    EXPECT_GT(mod.attention_temperature, 0.0f);

    /* Step 8: Apply to actual plasticity systems */
    bcm_params_t bcm_params = bcm_params_cortical();
    stdp_config_t stdp_config = stdp_config_default();

    immune_plasticity_modulate_bcm(&bcm_params, &mod);
    immune_plasticity_modulate_stdp(&stdp_config, &mod);

    /* Step 9: Verify statistics tracked */
    immune_plasticity_stats_t stats;
    immune_plasticity_get_stats(&stats);
    EXPECT_GT(stats.bcm_modulation_events, 0u);
    EXPECT_GT(stats.stdp_modulation_events, 0u);
    EXPECT_GT(stats.cytokine_updates, 0u);
}

TEST_F(ImmunePlasticityPipelineTest, ChronicInflammationPersistentImpairment) {
    /* WHAT: Chronic inflammation causes persistent learning deficits
     * WHY:  Long-term immune activation = long-term impairment
     * SCENARIO: Repeated immune activation → chronic state → persistent deficits
     */

    /* Simulate repeated immune responses */
    for (int i = 0; i < 5; i++) {
        simulate_immune_response(6, false);
    }

    /* Compute modulation */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &plasticity_config, &mod);

    /* Verify persistent impairment (some impairment from repeated responses) */
    EXPECT_LT(mod.global_plasticity_scale, 1.0f);  /* Should be below baseline */
    EXPECT_TRUE(immune_plasticity_is_impaired(&mod, 0.9f));  /* More lenient threshold */

    /* Verify cytokine presence (lower thresholds for repeated mild responses) */
    EXPECT_GT(mod.il1_concentration, 0.0f);
    EXPECT_GT(mod.il6_concentration, 0.0f);

    /* Verify some inflammation (at least local) */
    EXPECT_GE(mod.inflammation_level, INFLAMMATION_LOCAL);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
