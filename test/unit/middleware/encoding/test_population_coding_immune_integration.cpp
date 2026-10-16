/**
 * @file test_population_coding_immune_integration.cpp
 * @brief Unit tests for Population Coding-Immune Integration
 * @version 1.0.0
 * @date 2025-12-11
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_population_coding_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "utils/memory/nimcp_memory.h"

class PopulationImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    population_coding_encoder_t population_encoder;
    population_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Create population encoder */
        population_coding_config_t pop_config = population_coding_default_config();
        population_encoder = population_coding_create(&pop_config);
        ASSERT_NE(population_encoder, nullptr);

        /* Create bridge */
        population_immune_config_t bridge_config;
        population_immune_default_config(&bridge_config);
        bridge = population_immune_bridge_create(&bridge_config,
                                                  immune_system,
                                                  population_encoder);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            population_immune_bridge_destroy(bridge);
        }
        if (population_encoder) {
            population_coding_destroy(population_encoder);
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
        }
    }

    /* Helper: Present antigen to trigger immune response */
    void presentAntigen(uint32_t severity) {
        uint8_t epitope[64] = {0x01, 0x02, 0x03, 0x04};
        uint32_t antigen_id;
        brain_immune_present_antigen(immune_system,
                                      ANTIGEN_SOURCE_MANUAL,
                                      epitope, sizeof(epitope),
                                      severity, 0, &antigen_id);
    }

    /* Helper: Trigger inflammation */
    void triggerInflammation(brain_inflammation_level_t level) {
        uint32_t site_id;
        uint32_t antigen_id;
        presentAntigen(5);

        /* Get the antigen ID (last created) */
        if (immune_system->antigen_count > 0) {
            antigen_id = immune_system->antigens[0].id;
            brain_immune_initiate_inflammation(immune_system, 0, antigen_id, &site_id);

            /* Escalate to desired level */
            for (int i = 1; i < (int)level; i++) {
                brain_immune_escalate_inflammation(immune_system, site_id);
            }
        }
    }

    /* Helper: Release cytokine */
    void releaseCytokine(brain_cytokine_type_t type, float concentration) {
        uint32_t cytokine_id;
        brain_immune_release_cytokine(immune_system, type, 0,
                                       concentration, 0, &cytokine_id);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, DefaultConfig) {
    population_immune_config_t config;
    int result = population_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_noise_modulation);
    EXPECT_TRUE(config.enable_inflammation_tuning_modulation);
    EXPECT_TRUE(config.enable_population_anomaly_detection);
    EXPECT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_GT(config.baseline_precision, 0.9f);
}

TEST_F(PopulationImmuneTest, CreateDestroy) {
    /* Already created in SetUp */
    EXPECT_NE(bridge, nullptr);

    /* Check initial state */
    float health = population_immune_get_health_score(bridge);
    EXPECT_GT(health, 0.9f); /* Should start healthy */
}

TEST_F(PopulationImmuneTest, CreateWithNullPointers) {
    population_immune_bridge_t* null_bridge =
        population_immune_bridge_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

/* ============================================================================
 * Cytokine Effects Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, CytokineNoiseIncrease) {
    /* Baseline noise */
    float baseline_noise = population_immune_compute_noise(bridge);

    /* Release IL-1β (increases noise) */
    releaseCytokine(BRAIN_CYTOKINE_IL1, 0.8f);

    /* Apply effects */
    population_immune_apply_cytokine_effects(bridge);

    /* Check noise increased */
    float new_noise = population_immune_compute_noise(bridge);
    EXPECT_GT(new_noise, baseline_noise);
}

TEST_F(PopulationImmuneTest, CytokineGainReduction) {
    /* Baseline gain */
    float baseline_gain = population_immune_compute_gain(bridge);

    /* Release TNF-α (reduces gain) */
    releaseCytokine(BRAIN_CYTOKINE_TNF, 0.7f);

    /* Apply effects */
    population_immune_apply_cytokine_effects(bridge);

    /* Check gain reduced */
    float new_gain = population_immune_compute_gain(bridge);
    EXPECT_LT(new_gain, baseline_gain);
}

TEST_F(PopulationImmuneTest, CytokinePrecisionLoss) {
    /* Baseline precision */
    float baseline_precision = population_immune_compute_precision(bridge);

    /* Release IL-6 (reduces precision) */
    releaseCytokine(BRAIN_CYTOKINE_IL6, 0.6f);

    /* Apply effects */
    population_immune_apply_cytokine_effects(bridge);

    /* Check precision reduced */
    float new_precision = population_immune_compute_precision(bridge);
    EXPECT_LT(new_precision, baseline_precision);
}

TEST_F(PopulationImmuneTest, IL10Restoration) {
    /* First degrade with IL-6 */
    releaseCytokine(BRAIN_CYTOKINE_IL6, 0.8f);
    population_immune_apply_cytokine_effects(bridge);
    float degraded_precision = population_immune_compute_precision(bridge);

    /* Now release IL-10 (anti-inflammatory) */
    releaseCytokine(BRAIN_CYTOKINE_IL10, 0.9f);
    population_immune_apply_cytokine_effects(bridge);

    /* Check precision improved */
    float restored_precision = population_immune_compute_precision(bridge);
    EXPECT_GT(restored_precision, degraded_precision);
}

TEST_F(PopulationImmuneTest, MultipleCytokinesCombine) {
    /* Release multiple pro-inflammatory cytokines */
    releaseCytokine(BRAIN_CYTOKINE_IL1, 0.5f);
    releaseCytokine(BRAIN_CYTOKINE_IL6, 0.5f);
    releaseCytokine(BRAIN_CYTOKINE_TNF, 0.5f);

    /* Apply effects */
    population_immune_apply_cytokine_effects(bridge);

    /* Check combined effects */
    cytokine_population_effects_t effects;
    population_immune_get_cytokine_effects(bridge, &effects);

    EXPECT_GT(effects.total_noise_increase, 0.0f);
    EXPECT_GT(effects.total_precision_loss, 0.0f);
    EXPECT_GT(effects.total_gain_reduction, 0.0f);
}

/* ============================================================================
 * Inflammation Effects Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, InflammationIncreasesNoise) {
    /* Baseline noise */
    float baseline_noise = population_immune_compute_noise(bridge);

    /* Trigger regional inflammation */
    triggerInflammation(INFLAMMATION_REGIONAL);

    /* Apply effects */
    population_immune_apply_inflammation_effects(bridge);

    /* Check noise increased */
    float new_noise = population_immune_compute_noise(bridge);
    EXPECT_GT(new_noise, baseline_noise);
}

TEST_F(PopulationImmuneTest, InflammationReducesGain) {
    /* Baseline gain */
    float baseline_gain = population_immune_compute_gain(bridge);

    /* Trigger systemic inflammation */
    triggerInflammation(INFLAMMATION_SYSTEMIC);

    /* Apply effects */
    population_immune_apply_inflammation_effects(bridge);

    /* Check gain reduced */
    float new_gain = population_immune_compute_gain(bridge);
    EXPECT_LT(new_gain, baseline_gain);
}

TEST_F(PopulationImmuneTest, InflammationBroadensTuning) {
    /* Baseline tuning */
    float baseline_broadening = population_immune_compute_tuning_broadening(bridge);

    /* Trigger inflammation */
    triggerInflammation(INFLAMMATION_REGIONAL);

    /* Apply effects */
    population_immune_apply_inflammation_effects(bridge);

    /* Check tuning broadened */
    float new_broadening = population_immune_compute_tuning_broadening(bridge);
    EXPECT_GT(new_broadening, baseline_broadening);
}

TEST_F(PopulationImmuneTest, InflammationLevelScaling) {
    /* Test each inflammation level */
    brain_inflammation_level_t levels[] = {
        INFLAMMATION_LOCAL,
        INFLAMMATION_REGIONAL,
        INFLAMMATION_SYSTEMIC,
        INFLAMMATION_STORM
    };

    float prev_noise = 0.0f;
    for (size_t i = 0; i < 4; i++) {
        /* Reset */
        TearDown();
        SetUp();

        /* Trigger inflammation at level */
        triggerInflammation(levels[i]);
        population_immune_apply_inflammation_effects(bridge);

        float noise = population_immune_compute_noise(bridge);
        if (i > 0) {
            EXPECT_GT(noise, prev_noise); /* Higher levels = more noise */
        }
        prev_noise = noise;
    }
}

TEST_F(PopulationImmuneTest, InflammationReducesPrecision) {
    /* Baseline precision */
    float baseline_precision = population_immune_compute_precision(bridge);

    /* Trigger inflammation */
    triggerInflammation(INFLAMMATION_SYSTEMIC);

    /* Apply effects */
    population_immune_apply_inflammation_effects(bridge);

    /* Check precision reduced */
    float new_precision = population_immune_compute_precision(bridge);
    EXPECT_LT(new_precision, baseline_precision);
}

/* ============================================================================
 * Population Anomaly Detection Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, DetectHighNoise) {
    /* Simulate high noise population response */
    population_immune_detect_anomalies(bridge, 0.8f, 0.7f, 1.0f);

    /* Check noise trigger */
    population_immune_trigger_t trigger;
    memcpy(&trigger, &bridge->immune_trigger, sizeof(trigger));
    EXPECT_TRUE(trigger.noise_triggered);
}

TEST_F(PopulationImmuneTest, DetectLowSynchrony) {
    /* Simulate low synchrony population response */
    population_immune_detect_anomalies(bridge, 0.2f, 0.2f, 1.0f);

    /* Check synchrony trigger */
    population_immune_trigger_t trigger;
    memcpy(&trigger, &bridge->immune_trigger, sizeof(trigger));
    EXPECT_TRUE(trigger.synchrony_triggered);
}

TEST_F(PopulationImmuneTest, DetectGainAnomaly) {
    /* Simulate abnormal gain */
    population_immune_detect_anomalies(bridge, 0.2f, 0.7f, 0.3f);

    /* Check gain trigger */
    population_immune_trigger_t trigger;
    memcpy(&trigger, &bridge->immune_trigger, sizeof(trigger));
    EXPECT_TRUE(trigger.gain_triggered);
}

TEST_F(PopulationImmuneTest, AnomalyTriggersSeverity) {
    /* Trigger all anomalies */
    population_immune_detect_anomalies(bridge, 0.9f, 0.1f, 0.2f);

    /* Check high threat severity */
    float severity = bridge->immune_trigger.threat_severity;
    EXPECT_GT(severity, 0.5f);
}

TEST_F(PopulationImmuneTest, AnomalyTriggersImmuneResponse) {
    /* Initial antigen count */
    size_t initial_count = immune_system->antigen_count;

    /* Trigger severe anomaly */
    population_immune_detect_anomalies(bridge, 0.95f, 0.05f, 0.1f);
    population_immune_trigger_from_anomaly(bridge);

    /* Check antigen created */
    EXPECT_GT(immune_system->antigen_count, initial_count);
}

TEST_F(PopulationImmuneTest, NoAnomalyNoTrigger) {
    /* Normal population metrics */
    population_immune_detect_anomalies(bridge, 0.1f, 0.8f, 1.0f);

    /* Check no triggers */
    population_immune_trigger_t trigger;
    memcpy(&trigger, &bridge->immune_trigger, sizeof(trigger));
    EXPECT_FALSE(trigger.noise_triggered);
    EXPECT_FALSE(trigger.synchrony_triggered);
    EXPECT_FALSE(trigger.gain_triggered);
}

/* ============================================================================
 * Health Metrics Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, HealthMetricsBaseline) {
    /* Check baseline health */
    population_health_metrics_t metrics;
    population_immune_get_health_metrics(bridge, &metrics);

    EXPECT_GT(metrics.overall_health, 0.9f);
    EXPECT_LT(metrics.degradation_from_baseline, 0.1f);
    EXPECT_TRUE(metrics.fully_recovered);
}

TEST_F(PopulationImmuneTest, HealthDegradationFromCytokines) {
    /* Baseline health */
    float baseline_health = population_immune_get_health_score(bridge);

    /* Release pro-inflammatory cytokines */
    releaseCytokine(BRAIN_CYTOKINE_IL1, 0.8f);
    releaseCytokine(BRAIN_CYTOKINE_IL6, 0.8f);
    releaseCytokine(BRAIN_CYTOKINE_TNF, 0.8f);

    /* Update */
    population_immune_bridge_update(bridge, 100);

    /* Check health degraded */
    float new_health = population_immune_get_health_score(bridge);
    EXPECT_LT(new_health, baseline_health);
}

TEST_F(PopulationImmuneTest, HealthDegradationFromInflammation) {
    /* Baseline health */
    float baseline_health = population_immune_get_health_score(bridge);

    /* Trigger inflammation */
    triggerInflammation(INFLAMMATION_SYSTEMIC);

    /* Update */
    population_immune_bridge_update(bridge, 100);

    /* Check health degraded */
    float new_health = population_immune_get_health_score(bridge);
    EXPECT_LT(new_health, baseline_health);
}

TEST_F(PopulationImmuneTest, IsDegradedCheck) {
    /* Should not be degraded initially */
    EXPECT_FALSE(population_immune_is_degraded(bridge));

    /* Trigger severe inflammation */
    triggerInflammation(INFLAMMATION_STORM);
    population_immune_bridge_update(bridge, 100);

    /* Should now be degraded */
    EXPECT_TRUE(population_immune_is_degraded(bridge));
}

/* ============================================================================
 * Restoration Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, RestorationSignalOnRecovery) {
    /* Degrade system */
    releaseCytokine(BRAIN_CYTOKINE_IL6, 0.9f);
    population_immune_bridge_update(bridge, 100);

    /* Initial IL-10 count */
    size_t initial_il10_count = 0;
    for (size_t i = 0; i < immune_system->cytokine_count; i++) {
        if (immune_system->cytokines[i].type == BRAIN_CYTOKINE_IL10) {
            initial_il10_count++;
        }
    }

    /* Restore with IL-10 */
    releaseCytokine(BRAIN_CYTOKINE_IL10, 1.0f);
    population_immune_bridge_update(bridge, 100);

    /* Check health improved */
    float health = population_immune_get_health_score(bridge);
    EXPECT_GT(health, 0.5f);
}

/* ============================================================================
 * Bridge Update Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, UpdateAppliesAllEffects) {
    /* Trigger cytokines and inflammation */
    releaseCytokine(BRAIN_CYTOKINE_IL1, 0.5f);
    triggerInflammation(INFLAMMATION_REGIONAL);

    /* Update bridge */
    int result = population_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    /* Check all effects applied */
    population_health_metrics_t metrics;
    population_immune_get_health_metrics(bridge, &metrics);

    EXPECT_GT(metrics.noise, 0.1f);
    EXPECT_LT(metrics.gain, 1.0f);
    EXPECT_LT(metrics.precision, 1.0f);
}

TEST_F(PopulationImmuneTest, UpdateIncrementsTotalUpdates) {
    /* Initial count */
    uint64_t initial_updates = bridge->total_updates;

    /* Update several times */
    population_immune_bridge_update(bridge, 100);
    population_immune_bridge_update(bridge, 100);
    population_immune_bridge_update(bridge, 100);

    /* Check count increased */
    EXPECT_EQ(bridge->total_updates, initial_updates + 3);
}

/* ============================================================================
 * Advanced Integration Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, ModulateVectorDecoding) {
    /* Create test rates */
    const uint32_t num_neurons = 10;
    float rates[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        rates[i] = 50.0f; /* 50 Hz baseline */
    }

    /* Trigger inflammation to reduce gain */
    triggerInflammation(INFLAMMATION_REGIONAL);
    population_immune_bridge_update(bridge, 100);

    /* Modulate rates */
    float noisy_rates[num_neurons];
    int result = population_immune_modulate_vector_decoding(
        bridge, rates, nullptr, num_neurons, noisy_rates);

    EXPECT_EQ(result, 0);

    /* Check rates were modulated (not identical to input) */
    bool different = false;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (fabsf(noisy_rates[i] - rates[i]) > 1.0f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different);
}

TEST_F(PopulationImmuneTest, ModulateSynchrony) {
    /* Create baseline synchrony */
    synchrony_result_t baseline;
    baseline.synchrony_index = 0.8f;
    baseline.mean_correlation = 0.7f;
    baseline.coherence = 0.75f;
    baseline.peak_lag_ms = 5.0f;

    /* Trigger inflammation */
    triggerInflammation(INFLAMMATION_SYSTEMIC);
    population_immune_bridge_update(bridge, 100);

    /* Modulate synchrony */
    synchrony_result_t modulated;
    int result = population_immune_modulate_synchrony(bridge, &baseline, &modulated);

    EXPECT_EQ(result, 0);
    EXPECT_LT(modulated.synchrony_index, baseline.synchrony_index);
    EXPECT_LT(modulated.mean_correlation, baseline.mean_correlation);
}

TEST_F(PopulationImmuneTest, ModulateSparseCode) {
    /* Create baseline sparse code */
    const uint32_t num_neurons = 100;
    bool baseline_code[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        baseline_code[i] = (i < 10); /* 10% sparse */
    }

    /* Trigger inflammation to reduce reliability */
    triggerInflammation(INFLAMMATION_STORM);
    population_immune_bridge_update(bridge, 100);

    /* Modulate sparse code */
    bool noisy_code[num_neurons];
    int result = population_immune_modulate_sparse_code(
        bridge, baseline_code, num_neurons, noisy_code);

    EXPECT_EQ(result, 0);

    /* Check some bits flipped */
    uint32_t differences = 0;
    for (uint32_t i = 0; i < num_neurons; i++) {
        if (noisy_code[i] != baseline_code[i]) {
            differences++;
        }
    }
    EXPECT_GT(differences, 0); /* Should have some flips */
}

/* ============================================================================
 * Integration Statistics Tests
 * ============================================================================ */

TEST_F(PopulationImmuneTest, StatisticsTracking) {
    /* Initial stats */
    EXPECT_EQ(bridge->cytokine_modulations, 0);
    EXPECT_EQ(bridge->anomaly_detections, 0);
    EXPECT_EQ(bridge->immune_triggers, 0);

    /* Trigger cytokine effects */
    releaseCytokine(BRAIN_CYTOKINE_IL1, 0.5f);
    population_immune_apply_cytokine_effects(bridge);
    EXPECT_GT(bridge->cytokine_modulations, 0);

    /* Trigger anomaly */
    population_immune_detect_anomalies(bridge, 0.9f, 0.1f, 0.2f);
    EXPECT_GT(bridge->anomaly_detections, 0);

    /* Trigger immune response */
    population_immune_trigger_from_anomaly(bridge);
    EXPECT_GT(bridge->immune_triggers, 0);
}

/* ============================================================================
 * Edge Cases and Error Handling
 * ============================================================================ */

TEST_F(PopulationImmuneTest, NullPointerHandling) {
    EXPECT_EQ(population_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(population_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_EQ(population_immune_detect_anomalies(nullptr, 0, 0, 0), -1);
    EXPECT_EQ(population_immune_bridge_update(nullptr, 100), -1);
}

TEST_F(PopulationImmuneTest, DisabledFeaturesNoEffect) {
    /* Create bridge with all features disabled */
    population_immune_config_t config;
    population_immune_default_config(&config);
    config.enable_cytokine_noise_modulation = false;
    config.enable_inflammation_tuning_modulation = false;
    config.enable_population_anomaly_detection = false;

    population_immune_bridge_t* disabled_bridge =
        population_immune_bridge_create(&config, immune_system, population_encoder);
    ASSERT_NE(disabled_bridge, nullptr);

    /* Try to apply effects - should return 0 but do nothing */
    releaseCytokine(BRAIN_CYTOKINE_IL1, 0.9f);
    EXPECT_EQ(population_immune_apply_cytokine_effects(disabled_bridge), 0);

    /* Check no modulations counted */
    EXPECT_EQ(disabled_bridge->cytokine_modulations, 0);

    population_immune_bridge_destroy(disabled_bridge);
}

TEST_F(PopulationImmuneTest, ExtremeValuesHandling) {
    /* Test extreme cytokine levels */
    releaseCytokine(BRAIN_CYTOKINE_IL1, 10.0f); /* Way over 1.0 */
    population_immune_apply_cytokine_effects(bridge);

    /* Should clamp to reasonable values */
    float noise = population_immune_compute_noise(bridge);
    EXPECT_LE(noise, 1.0f);

    float gain = population_immune_compute_gain(bridge);
    EXPECT_GE(gain, 0.1f); /* Should not go to zero */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
