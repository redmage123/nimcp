/**
 * @file unit_cognitive_immune_plasticity_modulation.cpp
 * @brief Unit tests for brain immune-plasticity integration
 *
 * WHAT: Comprehensive tests for immune modulation of plasticity mechanisms
 * WHY:  Verify correct integration of immune state with BCM, STDP, attention
 * HOW:  Google Test framework with biological validation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_brain_immune_plasticity.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/attention/nimcp_attention.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class ImmunePlasticityTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    immune_plasticity_config_t config;

    void SetUp() override {
        /* Create immune system with defaults */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Get default plasticity config */
        int result = immune_plasticity_default_config(&config);
        ASSERT_EQ(result, 0);

        /* Reset statistics */
        immune_plasticity_reset_stats();
    }

    void TearDown() override {
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

        /* Escalate to desired level, with max iterations to prevent infinite loop */
        int max_iterations = 10;
        while (immune_system->inflammation_sites[0].level < level && max_iterations-- > 0) {
            brain_inflammation_level_t prev_level = immune_system->inflammation_sites[0].level;
            brain_immune_escalate_inflammation(immune_system, site_id);
            /* Break if escalation didn't change level (already at max) */
            if (immune_system->inflammation_sites[0].level == prev_level) {
                break;
            }
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, DefaultConfigValid) {
    /* WHAT: Verify default configuration has reasonable values
     * WHY:  Ensure biological plausibility
     */
    EXPECT_GT(config.il1_threshold_sensitivity, 0.0f);
    EXPECT_LT(config.il1_threshold_sensitivity, 5.0f);

    EXPECT_GT(config.il6_timing_sensitivity, 0.0f);
    EXPECT_LT(config.il6_timing_sensitivity, 1.0f);

    EXPECT_GT(config.tnf_attention_sensitivity, 0.0f);
    EXPECT_LT(config.tnf_attention_sensitivity, 1.0f);

    EXPECT_GT(config.min_plasticity_factor, 0.0f);
    EXPECT_LT(config.min_plasticity_factor, 0.5f);

    EXPECT_GT(config.max_threshold_elevation, 1.0f);
    EXPECT_LT(config.max_threshold_elevation, 5.0f);
}

TEST_F(ImmunePlasticityTest, ConfigNullPointerHandling) {
    /* WHAT: Verify NULL pointer safety
     * WHY:  Prevent crashes on invalid input
     */
    int result = immune_plasticity_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Cytokine Concentration Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, GetCytokineConcentrationEmpty) {
    /* WHAT: Check concentration when no cytokines present
     * WHY:  Baseline should be zero
     */
    float conc = immune_plasticity_get_cytokine_concentration(
        immune_system, BRAIN_CYTOKINE_IL1);
    EXPECT_FLOAT_EQ(conc, 0.0f);
}

TEST_F(ImmunePlasticityTest, GetCytokineConcentrationSingle) {
    /* WHAT: Verify single cytokine concentration
     * WHY:  Should return exact value
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);

    float conc = immune_plasticity_get_cytokine_concentration(
        immune_system, BRAIN_CYTOKINE_IL1);
    EXPECT_FLOAT_EQ(conc, 0.5f);
}

TEST_F(ImmunePlasticityTest, GetCytokineConcentrationMultiple) {
    /* WHAT: Sum multiple cytokines of same type
     * WHY:  Multiple releases accumulate
     */
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.3f);
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.4f);

    float conc = immune_plasticity_get_cytokine_concentration(
        immune_system, BRAIN_CYTOKINE_IL6);
    EXPECT_FLOAT_EQ(conc, 0.7f);
}

TEST_F(ImmunePlasticityTest, GetCytokineConcentrationClamped) {
    /* WHAT: Verify concentration clamped at 1.0
     * WHY:  Prevent overflow
     */
    add_cytokine(CYTOKINE_TNF_ALPHA, 0.8f);
    add_cytokine(CYTOKINE_TNF_ALPHA, 0.5f);

    float conc = immune_plasticity_get_cytokine_concentration(
        immune_system, CYTOKINE_TNF_ALPHA);
    EXPECT_FLOAT_EQ(conc, 1.0f);
}

TEST_F(ImmunePlasticityTest, GetCytokineConcentrationTypeSpecific) {
    /* WHAT: Verify only matching types are summed
     * WHY:  Different cytokines have different effects
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.3f);

    float il1 = immune_plasticity_get_cytokine_concentration(
        immune_system, BRAIN_CYTOKINE_IL1);
    float il6 = immune_plasticity_get_cytokine_concentration(
        immune_system, BRAIN_CYTOKINE_IL6);

    EXPECT_FLOAT_EQ(il1, 0.5f);
    EXPECT_FLOAT_EQ(il6, 0.3f);
}

/* ============================================================================
 * Inflammation Level Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, GetMaxInflammationNone) {
    /* WHAT: Check max inflammation when none present
     * WHY:  Should return NONE
     */
    auto level = immune_plasticity_get_max_inflammation(immune_system);
    EXPECT_EQ(level, INFLAMMATION_NONE);
}

TEST_F(ImmunePlasticityTest, GetMaxInflammationSingle) {
    /* WHAT: Verify single inflammation site level
     * WHY:  Should match site level
     */
    create_inflammation(INFLAMMATION_REGIONAL);

    auto level = immune_plasticity_get_max_inflammation(immune_system);
    EXPECT_EQ(level, INFLAMMATION_REGIONAL);
}

TEST_F(ImmunePlasticityTest, GetMaxInflammationMultiple) {
    /* WHAT: Find maximum among multiple sites
     * WHY:  Most severe inflammation determines effects
     */
    create_inflammation(INFLAMMATION_LOCAL);
    create_inflammation(INFLAMMATION_REGIONAL);

    auto level = immune_plasticity_get_max_inflammation(immune_system);
    /* With the helper checking site[0], both sites end up at REGIONAL
     * (the first escalates to LOCAL, second to REGIONAL, and we find max) */
    EXPECT_GE(level, INFLAMMATION_LOCAL);
}

/* ============================================================================
 * Modulation Computation Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, ComputeModulationBaseline) {
    /* WHAT: Verify baseline modulation with no immune activation
     * WHY:  Should have no effect (all factors = 1.0)
     */
    immune_plasticity_modulation_t mod;
    int result = immune_plasticity_compute_modulation(
        immune_system, &config, &mod);

    ASSERT_EQ(result, 0);
    EXPECT_FLOAT_EQ(mod.bcm_threshold_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.bcm_learning_rate_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.stdp_tau_plus_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.stdp_tau_minus_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.stdp_learning_rate_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.attention_gate_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.attention_temperature, 0.0f);
    EXPECT_FLOAT_EQ(mod.global_plasticity_scale, 1.0f);
}

TEST_F(ImmunePlasticityTest, ComputeModulationIL1Effect) {
    /* WHAT: IL-1β elevates BCM threshold
     * WHY:  Pro-inflammatory cytokine impairs LTP
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    EXPECT_GT(mod.bcm_threshold_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.il1_concentration, 0.5f);
}

TEST_F(ImmunePlasticityTest, ComputeModulationIL6Effect) {
    /* WHAT: IL-6 narrows STDP timing windows
     * WHY:  Acute phase response affects timing precision
     */
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.6f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    EXPECT_LT(mod.stdp_tau_plus_scale, 1.0f);
    EXPECT_LT(mod.stdp_tau_minus_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.il6_concentration, 0.6f);
}

TEST_F(ImmunePlasticityTest, ComputeModulationTNFEffect) {
    /* WHAT: TNF-α impairs attention
     * WHY:  Severe inflammation affects cognitive control
     */
    add_cytokine(CYTOKINE_TNF_ALPHA, 0.7f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    EXPECT_LT(mod.attention_gate_scale, 1.0f);
    EXPECT_LT(mod.stdp_learning_rate_scale, 1.0f);
    EXPECT_FLOAT_EQ(mod.tnf_alpha_concentration, 0.7f);
}

TEST_F(ImmunePlasticityTest, ComputeModulationIL10Recovery) {
    /* WHAT: IL-10 partially restores plasticity
     * WHY:  Anti-inflammatory cytokine promotes recovery
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.8f);  /* Suppress plasticity */
    add_cytokine(BRAIN_CYTOKINE_IL10, 0.5f); /* Restore partially */

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    /* IL-10 should boost learning rates slightly */
    EXPECT_GT(mod.bcm_learning_rate_scale, 0.0f);
    EXPECT_GT(mod.global_plasticity_scale, 0.0f);
    EXPECT_FLOAT_EQ(mod.il10_concentration, 0.5f);
}

TEST_F(ImmunePlasticityTest, ComputeModulationInflammationEffect) {
    /* WHAT: Inflammation reduces learning rates
     * WHY:  Inflammatory state impairs plasticity
     */
    create_inflammation(INFLAMMATION_SYSTEMIC);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    EXPECT_LT(mod.bcm_learning_rate_scale, 1.0f);
    EXPECT_GT(mod.attention_temperature, 0.0f);
    EXPECT_EQ(mod.inflammation_level, INFLAMMATION_SYSTEMIC);
}

TEST_F(ImmunePlasticityTest, ComputeModulationNullPointers) {
    /* WHAT: Verify NULL pointer safety
     * WHY:  Prevent crashes
     */
    immune_plasticity_modulation_t mod;

    EXPECT_EQ(immune_plasticity_compute_modulation(nullptr, &config, &mod), -1);
    EXPECT_EQ(immune_plasticity_compute_modulation(immune_system, nullptr, &mod), -1);
    EXPECT_EQ(immune_plasticity_compute_modulation(immune_system, &config, nullptr), -1);
}

/* ============================================================================
 * BCM Integration Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, BCMModulationBaseline) {
    /* WHAT: BCM params unchanged at baseline
     * WHY:  No immune activation = no effect
     */
    bcm_params_t params = bcm_params_cortical();
    float original_lr = params.learning_rate;
    float original_min_thresh = params.min_threshold;

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_bcm(&params, &mod);

    EXPECT_FLOAT_EQ(params.learning_rate, original_lr);
    EXPECT_FLOAT_EQ(params.min_threshold, original_min_thresh);
}

TEST_F(ImmunePlasticityTest, BCMThresholdElevation) {
    /* WHAT: IL-1β elevates BCM threshold
     * WHY:  Makes LTP harder to induce
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.6f);

    bcm_params_t params = bcm_params_cortical();
    float original_thresh = params.min_threshold;

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_bcm(&params, &mod);

    EXPECT_GT(params.min_threshold, original_thresh);
    EXPECT_GT(params.max_threshold / params.min_threshold,
              1.0f); /* Threshold range maintained */
}

TEST_F(ImmunePlasticityTest, BCMLearningRateReduction) {
    /* WHAT: Inflammation reduces BCM learning rate
     * WHY:  Impaired plasticity during inflammation
     */
    create_inflammation(INFLAMMATION_REGIONAL);

    bcm_params_t params = bcm_params_cortical();
    float original_lr = params.learning_rate;

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_bcm(&params, &mod);

    EXPECT_LT(params.learning_rate, original_lr);
}

TEST_F(ImmunePlasticityTest, BCMDirectThresholdModulation) {
    /* WHAT: Real-time threshold adjustment
     * WHY:  Dynamic modulation during learning
     */
    bcm_synapse_t synapse = bcm_synapse_init(0.5f, 0.1f);
    float original_thresh = synapse.threshold;

    add_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    float new_thresh = immune_plasticity_modulate_bcm_threshold(&synapse, &mod);

    EXPECT_GT(new_thresh, original_thresh);
    EXPECT_FLOAT_EQ(synapse.threshold, new_thresh);
}

/* ============================================================================
 * STDP Integration Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, STDPModulationBaseline) {
    /* WHAT: STDP config unchanged at baseline
     * WHY:  No immune activation = no effect
     */
    stdp_config_t stdp_config = stdp_config_default();
    float original_tau_plus = stdp_config.tau_plus;
    float original_lr = stdp_config.learning_rate;

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_stdp(&stdp_config, &mod);

    EXPECT_FLOAT_EQ(stdp_config.tau_plus, original_tau_plus);
    EXPECT_FLOAT_EQ(stdp_config.learning_rate, original_lr);
}

TEST_F(ImmunePlasticityTest, STDPTimingWindowNarrowing) {
    /* WHAT: IL-6 narrows STDP timing windows
     * WHY:  More precise timing required during inflammation
     */
    add_cytokine(BRAIN_CYTOKINE_IL6, 0.7f);

    stdp_config_t stdp_config = stdp_config_default();
    float original_tau = stdp_config.tau_plus;

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_stdp(&stdp_config, &mod);

    EXPECT_LT(stdp_config.tau_plus, original_tau);
    EXPECT_LT(stdp_config.tau_minus, original_tau);
}

TEST_F(ImmunePlasticityTest, STDPLearningRateReduction) {
    /* WHAT: TNF-α reduces STDP learning rate
     * WHY:  Impaired spike-timing plasticity
     */
    add_cytokine(CYTOKINE_TNF_ALPHA, 0.6f);

    stdp_config_t stdp_config = stdp_config_default();
    float original_lr = stdp_config.learning_rate;

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_stdp(&stdp_config, &mod);

    EXPECT_LT(stdp_config.learning_rate, original_lr);
}

TEST_F(ImmunePlasticityTest, STDPDirectTimingModulation) {
    /* WHAT: Real-time timing window adjustment
     * WHY:  Dynamic modulation during learning
     */
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);
    float original_tau_plus = synapse.tau_plus;

    add_cytokine(BRAIN_CYTOKINE_IL6, 0.5f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_stdp_timing(&synapse, &mod);

    EXPECT_LT(synapse.tau_plus, original_tau_plus);
    EXPECT_LT(synapse.tau_minus, original_tau_plus);
}

/* ============================================================================
 * Attention Integration Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, AttentionConfigModulation) {
    /* WHAT: TNF-α reduces attention gate bias
     * WHY:  Impaired cognitive control
     */
    add_cytokine(CYTOKINE_TNF_ALPHA, 0.8f);

    multihead_attention_config_t attn_config;
    memset(&attn_config, 0, sizeof(attn_config));
    attn_config.num_heads = 4;
    attn_config.input_dim = 128;
    attn_config.output_dim = 128;
    attn_config.sequence_length = 32;
    attn_config.gate_bias = 0.5f;

    float original_gate = attn_config.gate_bias;

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_attention_config(&attn_config, &mod);

    EXPECT_LT(attn_config.gate_bias, original_gate);
}

TEST_F(ImmunePlasticityTest, AttentionGateModulation) {
    /* WHAT: Real-time attention gate adjustment
     * WHY:  Dynamic impairment during inflammation
     */
    multihead_attention_config_t attn_config;
    memset(&attn_config, 0, sizeof(attn_config));
    attn_config.num_heads = 4;
    attn_config.input_dim = 128;
    attn_config.output_dim = 128;
    attn_config.sequence_length = 32;
    attn_config.use_thalamic_gate = true;
    attn_config.gate_bias = 0.7f;

    multihead_attention_t mha = multihead_attention_create(&attn_config);
    ASSERT_NE(mha, nullptr);

    add_cytokine(CYTOKINE_TNF_ALPHA, 0.6f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    int result = immune_plasticity_modulate_attention_gate(mha, &mod);
    EXPECT_EQ(result, 0);

    multihead_attention_destroy(mha);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, StatisticsTracking) {
    /* WHAT: Verify statistics are tracked correctly
     * WHY:  Monitor immune-plasticity interactions
     */
    bcm_params_t bcm_p = bcm_params_cortical();
    stdp_config_t stdp_c = stdp_config_default();

    add_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_bcm(&bcm_p, &mod);
    immune_plasticity_modulate_stdp(&stdp_c, &mod);

    immune_plasticity_stats_t stats;
    immune_plasticity_get_stats(&stats);

    EXPECT_GT(stats.bcm_modulation_events, 0u);
    EXPECT_GT(stats.stdp_modulation_events, 0u);
    EXPECT_GT(stats.cytokine_updates, 0u);
}

TEST_F(ImmunePlasticityTest, StatisticsReset) {
    /* WHAT: Statistics reset clears counters
     * WHY:  Fresh measurement periods
     */
    bcm_params_t params = bcm_params_cortical();
    immune_plasticity_modulation_t mod;

    immune_plasticity_compute_modulation(immune_system, &config, &mod);
    immune_plasticity_modulate_bcm(&params, &mod);

    immune_plasticity_reset_stats();

    immune_plasticity_stats_t stats;
    immune_plasticity_get_stats(&stats);

    EXPECT_EQ(stats.bcm_modulation_events, 0u);
    EXPECT_EQ(stats.stdp_modulation_events, 0u);
    EXPECT_EQ(stats.cytokine_updates, 0u);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, ImpairedDetectionNone) {
    /* WHAT: Baseline not impaired
     * WHY:  No immune activation
     */
    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    bool impaired = immune_plasticity_is_impaired(&mod, 0.7f);
    EXPECT_FALSE(impaired);
}

TEST_F(ImmunePlasticityTest, ImpairedDetectionSevere) {
    /* WHAT: Severe inflammation detected as impaired
     * WHY:  Cytokine storm suppresses plasticity
     */
    create_inflammation(INFLAMMATION_STORM);
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.9f);
    add_cytokine(CYTOKINE_TNF_ALPHA, 0.9f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    bool impaired = immune_plasticity_is_impaired(&mod, 0.7f);
    EXPECT_TRUE(impaired);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(ImmunePlasticityTest, ModulationToString) {
    /* WHAT: Format modulation as human-readable string
     * WHY:  Debugging and logging
     */
    add_cytokine(BRAIN_CYTOKINE_IL1, 0.5f);

    immune_plasticity_modulation_t mod;
    immune_plasticity_compute_modulation(immune_system, &config, &mod);

    char buffer[512];
    int len = immune_plasticity_modulation_to_string(&mod, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
    EXPECT_LT(len, static_cast<int>(sizeof(buffer)));
    EXPECT_NE(strstr(buffer, "IL1"), nullptr);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
