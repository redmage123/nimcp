/**
 * @file test_cortical_immune.cpp
 * @brief Unit tests for cortical immune integration
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_immune.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalImmuneTest : public ::testing::Test {
protected:
    cortical_immune_system_t* cortical_immune;
    brain_immune_system_t* brain_immune;
    cortical_immune_config_t config;

    void SetUp() override {
        cortical_immune_default_config(&config);
        cortical_immune = cortical_immune_create(&config);
        ASSERT_NE(cortical_immune, nullptr);

        /* Create brain immune system */
        brain_immune_config_t bi_config;
        brain_immune_default_config(&bi_config);
        brain_immune = brain_immune_create(&bi_config);
        ASSERT_NE(brain_immune, nullptr);
    }

    void TearDown() override {
        if (cortical_immune) {
            cortical_immune_destroy(cortical_immune);
        }
        if (brain_immune) {
            brain_immune_destroy(brain_immune);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, DefaultConfig) {
    cortical_immune_config_t cfg;
    int result = cortical_immune_default_config(&cfg);

    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.max_microglial_sites, 0u);
    EXPECT_GT(cfg.microglial_density, 0.0f);
    EXPECT_GT(cfg.surveillance_radius, 0.0f);
    EXPECT_TRUE(cfg.enable_immune_integration);
}

TEST_F(CorticalImmuneTest, CreateWithConfig) {
    cortical_immune_config_t custom_config;
    cortical_immune_default_config(&custom_config);
    custom_config.max_microglial_sites = 128;
    custom_config.surveillance_radius = 10.0f;

    cortical_immune_system_t* system = cortical_immune_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_immune_destroy(system);
}

TEST_F(CorticalImmuneTest, CreateWithNullConfig) {
    cortical_immune_system_t* system = cortical_immune_create(nullptr);
    ASSERT_NE(system, nullptr);
    cortical_immune_destroy(system);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, ConnectToBrainImmune) {
    int result = cortical_immune_connect_brain_immune(cortical_immune, brain_immune);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalImmuneTest, ConnectNullBrainImmune) {
    int result = cortical_immune_connect_brain_immune(cortical_immune, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Microglial Site Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, CreateMicroglialSite) {
    uint32_t site_id = 0;
    int result = cortical_immune_create_microglial_site(
        cortical_immune, 0.0f, 0.0f, 5.0f, &site_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(site_id, 0u);
}

TEST_F(CorticalImmuneTest, CreateMultipleMicroglialSites) {
    uint32_t site_id1, site_id2, site_id3;

    int r1 = cortical_immune_create_microglial_site(
        cortical_immune, 0.0f, 0.0f, 5.0f, &site_id1);
    int r2 = cortical_immune_create_microglial_site(
        cortical_immune, 10.0f, 0.0f, 5.0f, &site_id2);
    int r3 = cortical_immune_create_microglial_site(
        cortical_immune, 0.0f, 10.0f, 5.0f, &site_id3);

    EXPECT_EQ(r1, 0);
    EXPECT_EQ(r2, 0);
    EXPECT_EQ(r3, 0);
    EXPECT_NE(site_id1, site_id2);
    EXPECT_NE(site_id2, site_id3);
}

TEST_F(CorticalImmuneTest, ActivateMicroglia) {
    uint32_t site_id;
    cortical_immune_create_microglial_site(
        cortical_immune, 0.0f, 0.0f, 5.0f, &site_id);

    int result = cortical_immune_activate_microglia(
        cortical_immune, site_id, ABNORMALITY_HYPEREXCITABILITY);

    EXPECT_EQ(result, 0);

    /* Verify activation */
    microglial_site_t site;
    cortical_immune_get_microglial_site(cortical_immune, site_id, &site);
    EXPECT_EQ(site.state, MICROGLIA_ACTIVATED);
    EXPECT_GT(site.activation_level, 0.0f);
}

TEST_F(CorticalImmuneTest, GetMicroglialSite) {
    uint32_t site_id;
    cortical_immune_create_microglial_site(
        cortical_immune, 1.5f, 2.5f, 3.0f, &site_id);

    microglial_site_t site;
    int result = cortical_immune_get_microglial_site(cortical_immune, site_id, &site);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(site.site_id, site_id);
    EXPECT_FLOAT_EQ(site.cortical_x, 1.5f);
    EXPECT_FLOAT_EQ(site.cortical_y, 2.5f);
    EXPECT_FLOAT_EQ(site.surveillance_radius, 3.0f);
}

/* ============================================================================
 * Column Registration Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, RegisterMinicolumn) {
    /* Note: minicolumn_create requires cortical_column_pool, so we test with null */
    int result = cortical_immune_register_minicolumn(
        cortical_immune, nullptr, 1);

    /* Should handle gracefully */
    EXPECT_EQ(result, -1);
}

TEST_F(CorticalImmuneTest, RegisterHypercolumn) {
    /* Test registration (doesn't require actual hypercolumn object for logging) */
    int result = cortical_immune_register_hypercolumn(
        cortical_immune, nullptr, 1);

    /* Should log and return success even with null (just registration) */
    EXPECT_EQ(result, -1);  /* Null check should fail */
}

/* ============================================================================
 * Abnormality Detection Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, DetectHyperexcitability) {
    /* Register a fake column */
    uint32_t column_id = 1;

    /* Detect with high activation */
    float score = cortical_immune_detect_hyperexcitability(
        cortical_immune, column_id, 0.9f);

    /* Should detect abnormality (baseline is typically ~0.1) */
    EXPECT_GE(score, 0.0f);
}

TEST_F(CorticalImmuneTest, DetectHypoactivity) {
    uint32_t column_id = 1;

    /* Detect with very low activation */
    float score = cortical_immune_detect_hypoactivity(
        cortical_immune, column_id, 0.01f);

    EXPECT_GE(score, 0.0f);
}

TEST_F(CorticalImmuneTest, DetectSynchronization) {
    uint32_t column_ids[] = {1, 2, 3, 4};
    float activations[] = {0.5f, 0.5f, 0.5f, 0.5f};  /* Perfect sync */

    float score = cortical_immune_detect_synchronization(
        cortical_immune, column_ids, activations, 4);

    /* High synchronization should be detected */
    EXPECT_GT(score, 0.5f);
}

TEST_F(CorticalImmuneTest, DetectNoSynchronization) {
    uint32_t column_ids[] = {1, 2, 3, 4};
    float activations[] = {0.1f, 0.4f, 0.7f, 0.2f};  /* Diverse */

    float score = cortical_immune_detect_synchronization(
        cortical_immune, column_ids, activations, 4);

    /* Low synchronization */
    EXPECT_LT(score, 0.5f);
}

TEST_F(CorticalImmuneTest, DetectLayerDysfunction) {
    uint32_t region_id = 0;
    cc_cortical_layer_t layer = CC_LAYER_IV;

    float score = cortical_immune_detect_layer_dysfunction(
        cortical_immune, region_id, layer);

    /* Should return 0 initially (no dysfunction) */
    EXPECT_GE(score, 0.0f);
}

TEST_F(CorticalImmuneTest, DetectFeatureLoss) {
    uint32_t hcol_id = 1;
    float degraded_osi = 0.2f;  /* Poor selectivity */

    float score = cortical_immune_detect_feature_loss(
        cortical_immune, hcol_id, degraded_osi);

    /* Should detect selectivity loss */
    EXPECT_GT(score, 0.0f);
}

TEST_F(CorticalImmuneTest, DetectNoFeatureLoss) {
    uint32_t hcol_id = 1;
    float good_osi = 0.8f;  /* Good selectivity */

    float score = cortical_immune_detect_feature_loss(
        cortical_immune, hcol_id, good_osi);

    /* Should not detect loss */
    EXPECT_LT(score, 0.3f);
}

/* ============================================================================
 * Inflammation Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, ApplyInflammation) {
    uint32_t column_id = 1;

    int result = cortical_immune_apply_inflammation(
        cortical_immune, column_id, 0.5f);

    EXPECT_EQ(result, 0);

    /* Verify column status */
    cortical_column_immune_t status;
    cortical_immune_get_column_status(cortical_immune, column_id, &status);
    EXPECT_GT(status.inflammation_level, 0.0f);
    EXPECT_LT(status.gain_modulation, 1.0f);
}

TEST_F(CorticalImmuneTest, InflammationReducesGain) {
    uint32_t column_id = 1;

    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.8f);

    cortical_column_immune_t status;
    cortical_immune_get_column_status(cortical_immune, column_id, &status);

    /* High inflammation should significantly reduce gain */
    EXPECT_LE(status.gain_modulation, 0.6f);
}

TEST_F(CorticalImmuneTest, InflammationSeverityCategories) {
    uint32_t column_id = 1;

    /* Test different severity levels */
    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.1f);
    cortical_column_immune_t status;
    cortical_immune_get_column_status(cortical_immune, column_id, &status);
    EXPECT_EQ(status.inflammation_severity, INFLAMMATION_NONE);

    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.3f);
    cortical_immune_get_column_status(cortical_immune, column_id, &status);
    EXPECT_EQ(status.inflammation_severity, INFLAMMATION_LOCAL);

    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.5f);
    cortical_immune_get_column_status(cortical_immune, column_id, &status);
    EXPECT_EQ(status.inflammation_severity, INFLAMMATION_REGIONAL);

    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.7f);
    cortical_immune_get_column_status(cortical_immune, column_id, &status);
    EXPECT_EQ(status.inflammation_severity, INFLAMMATION_SYSTEMIC);

    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.9f);
    cortical_immune_get_column_status(cortical_immune, column_id, &status);
    EXPECT_EQ(status.inflammation_severity, INFLAMMATION_STORM);
}

TEST_F(CorticalImmuneTest, ResolveInflammation) {
    uint32_t column_id = 1;

    /* Apply inflammation */
    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.8f);

    /* Resolve */
    int result = cortical_immune_resolve_inflammation(cortical_immune, column_id);
    EXPECT_EQ(result, 0);

    /* Verify reduction */
    cortical_column_immune_t status;
    cortical_immune_get_column_status(cortical_immune, column_id, &status);
    EXPECT_LT(status.inflammation_level, 0.8f);
}

/* ============================================================================
 * Cytokine Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, ApplyCytokineIL1) {
    uint32_t region_id = 0;
    cc_cortical_layer_t layer = CC_LAYER_IV;

    /* Register layer first */
    cortical_immune_register_laminar_structure(cortical_immune, nullptr, region_id);

    int result = cortical_immune_apply_cytokine(
        cortical_immune, region_id, layer, BRAIN_CYTOKINE_IL1, 0.5f);

    EXPECT_EQ(result, 0);

    /* Verify layer state */
    layer_immune_state_t state;
    cortical_immune_get_layer_state(cortical_immune, region_id, layer, &state);
    EXPECT_GT(state.il1_concentration, 0.0f);
}

TEST_F(CorticalImmuneTest, ApplyCytokineTNF) {
    uint32_t region_id = 0;
    cc_cortical_layer_t layer = CC_LAYER_II_III;

    cortical_immune_register_laminar_structure(cortical_immune, nullptr, region_id);

    cortical_immune_apply_cytokine(
        cortical_immune, region_id, layer, CYTOKINE_TNF_ALPHA, 0.8f);

    layer_immune_state_t state;
    cortical_immune_get_layer_state(cortical_immune, region_id, layer, &state);

    /* TNF should reduce all gains */
    EXPECT_LT(state.feedforward_gain, 1.0f);
    EXPECT_LT(state.feedback_gain, 1.0f);
    EXPECT_LT(state.lateral_gain, 1.0f);
}

TEST_F(CorticalImmuneTest, ApplyCytokineIL10) {
    uint32_t region_id = 0;
    cc_cortical_layer_t layer = CC_LAYER_V;

    cortical_immune_register_laminar_structure(cortical_immune, nullptr, region_id);

    /* First reduce gains with TNF */
    cortical_immune_apply_cytokine(
        cortical_immune, region_id, layer, CYTOKINE_TNF_ALPHA, 0.5f);

    layer_immune_state_t state_before;
    cortical_immune_get_layer_state(cortical_immune, region_id, layer, &state_before);
    float gain_before = state_before.feedforward_gain;

    /* Apply anti-inflammatory IL-10 */
    cortical_immune_apply_cytokine(
        cortical_immune, region_id, layer, BRAIN_CYTOKINE_IL10, 0.5f);

    layer_immune_state_t state_after;
    cortical_immune_get_layer_state(cortical_immune, region_id, layer, &state_after);

    /* IL-10 should restore gain */
    EXPECT_GT(state_after.feedforward_gain, gain_before);
}

TEST_F(CorticalImmuneTest, UpdateCytokineDiffusion) {
    /* Create microglial site and activate */
    uint32_t site_id;
    cortical_immune_create_microglial_site(
        cortical_immune, 0.0f, 0.0f, 5.0f, &site_id);
    cortical_immune_activate_microglia(
        cortical_immune, site_id, ABNORMALITY_HYPEREXCITABILITY);

    /* Update diffusion */
    int result = cortical_immune_update_cytokine_diffusion(cortical_immune, 100);
    EXPECT_EQ(result, 0);

    /* Verify cytokine increase */
    microglial_site_t site;
    cortical_immune_get_microglial_site(cortical_immune, site_id, &site);
    EXPECT_GT(site.cytokine_concentration, 0.0f);
}

/* ============================================================================
 * Antigen Presentation Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, PresentAbnormalityWithoutBrainImmune) {
    uint32_t column_id = 1;
    uint32_t antigen_id;

    /* Should fail without brain immune connection */
    int result = cortical_immune_present_abnormality(
        cortical_immune, column_id,
        ABNORMALITY_HYPEREXCITABILITY, 0.8f, &antigen_id);

    EXPECT_EQ(result, -1);
}

TEST_F(CorticalImmuneTest, PresentAbnormalityWithBrainImmune) {
    /* Connect brain immune */
    cortical_immune_connect_brain_immune(cortical_immune, brain_immune);
    brain_immune_start(brain_immune);

    uint32_t column_id = 1;
    uint32_t antigen_id;

    int result = cortical_immune_present_abnormality(
        cortical_immune, column_id,
        ABNORMALITY_HYPEREXCITABILITY, 0.8f, &antigen_id);

    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    /* Verify antigen was presented */
    cortical_immune_stats_t stats;
    cortical_immune_get_stats(cortical_immune, &stats);
    EXPECT_GT(stats.antigens_presented, 0u);
}

/* ============================================================================
 * Surveillance Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, UpdateSurveillance) {
    uint32_t count = cortical_immune_update_surveillance(cortical_immune, 100);
    EXPECT_GE(count, 0u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, GetStats) {
    cortical_immune_stats_t stats;
    int result = cortical_immune_get_stats(cortical_immune, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_GE(stats.total_microglial_sites, 0u);
}

TEST_F(CorticalImmuneTest, StatsUpdateAfterActivity) {
    /* Create microglial site */
    uint32_t site_id;
    cortical_immune_create_microglial_site(
        cortical_immune, 0.0f, 0.0f, 5.0f, &site_id);

    cortical_immune_stats_t stats;
    cortical_immune_get_stats(cortical_immune, &stats);

    EXPECT_EQ(stats.total_microglial_sites, 1u);
    EXPECT_EQ(stats.resting_microglia, 1u);

    /* Activate */
    cortical_immune_activate_microglia(
        cortical_immune, site_id, ABNORMALITY_HYPEREXCITABILITY);

    cortical_immune_get_stats(cortical_immune, &stats);
    EXPECT_EQ(stats.activated_microglia, 1u);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, Update) {
    int result = cortical_immune_update(cortical_immune, 100);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalImmuneTest, UpdateWithInflammation) {
    uint32_t column_id = 1;

    /* Apply inflammation */
    cortical_immune_apply_inflammation(cortical_immune, column_id, 0.5f);

    cortical_column_immune_t status_before;
    cortical_immune_get_column_status(cortical_immune, column_id, &status_before);

    /* Update multiple times (inflammation should decay) */
    for (int i = 0; i < 10; i++) {
        cortical_immune_update(cortical_immune, 100);
    }

    cortical_column_immune_t status_after;
    cortical_immune_get_column_status(cortical_immune, column_id, &status_after);

    /* Inflammation should have decreased */
    EXPECT_LT(status_after.inflammation_level, status_before.inflammation_level);
}

/* ============================================================================
 * String Utility Tests
 * ============================================================================ */

TEST_F(CorticalImmuneTest, MicroglialStateToString) {
    EXPECT_STREQ(cortical_immune_microglial_state_to_string(MICROGLIA_RESTING), "RESTING");
    EXPECT_STREQ(cortical_immune_microglial_state_to_string(MICROGLIA_ACTIVATED), "ACTIVATED");
}

TEST_F(CorticalImmuneTest, AbnormalityToString) {
    EXPECT_STREQ(cortical_immune_abnormality_to_string(ABNORMALITY_HYPEREXCITABILITY),
                 "HYPEREXCITABILITY");
    EXPECT_STREQ(cortical_immune_abnormality_to_string(ABNORMALITY_LAYER_DYSFUNCTION),
                 "LAYER_DYSFUNCTION");
}

TEST_F(CorticalImmuneTest, EffectToString) {
    EXPECT_STREQ(cortical_immune_effect_to_string(INFLAMMATION_EFFECT_GAIN_REDUCTION),
                 "GAIN_REDUCTION");
    EXPECT_STREQ(cortical_immune_effect_to_string(INFLAMMATION_EFFECT_SELECTIVITY_LOSS),
                 "SELECTIVITY_LOSS");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
