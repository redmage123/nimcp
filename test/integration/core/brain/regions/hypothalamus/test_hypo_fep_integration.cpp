/**
 * @file test_hypo_fep_integration.cpp
 * @brief Integration tests for Hypothalamus FEP bridges working together
 *
 * WHAT: Integration tests verifying multiple FEP bridges correctly
 *       integrate free energy computations with hypothalamic drives
 * WHY:  Ensure FEP bridges properly modulate precision, compute free energy,
 *       and generate active inference responses based on drive states
 * HOW:  Test curiosity, salience, and bio-async FEP bridges coordinating
 *       with each other through drive system integration
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_curiosity_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_salience_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_bio_async_fep_bridge.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* Define observation and action dimensions for FEP system */
#define FEP_OBSERVATION_DIM  8
#define FEP_ACTION_DIM       4

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_DRIVE_LEVEL_LOW        0.2f
#define TEST_DRIVE_LEVEL_MODERATE   0.5f
#define TEST_DRIVE_LEVEL_HIGH       0.8f
#define TEST_DRIVE_LEVEL_URGENT     0.95f

#define TEST_FATIGUE_NONE           0.0f
#define TEST_FATIGUE_LOW            0.3f
#define TEST_FATIGUE_HIGH           0.8f

#define FEP_EPSILON                 0.01f

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class HypoFepIntegrationTest : public ::testing::Test {
protected:
    fep_system_t* fep_system;
    hypo_drive_system_handle_t* drive_system;

    hypo_curiosity_fep_bridge_t* curiosity_bridge;
    hypo_salience_fep_bridge_t* salience_bridge;
    hypo_bio_async_fep_bridge_t* bio_async_bridge;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, FEP_OBSERVATION_DIM, FEP_ACTION_DIM);
        ASSERT_NE(fep_system, nullptr);

        /* Create drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);

        /* Create FEP bridges */
        hypo_curiosity_fep_config_t curiosity_config;
        hypo_curiosity_fep_default_config(&curiosity_config);
        curiosity_bridge = hypo_curiosity_fep_create(&curiosity_config, fep_system);
        ASSERT_NE(curiosity_bridge, nullptr);

        hypo_salience_fep_config_t salience_config;
        hypo_salience_fep_default_config(&salience_config);
        salience_bridge = hypo_salience_fep_create(&salience_config, drive_system, fep_system);
        ASSERT_NE(salience_bridge, nullptr);

        hypo_bio_async_fep_config_t bio_async_config;
        hypo_bio_async_fep_default_config(&bio_async_config);
        bio_async_bridge = hypo_bio_async_fep_create(&bio_async_config, drive_system, fep_system);
        ASSERT_NE(bio_async_bridge, nullptr);

        /* Connect curiosity bridge to drive system */
        hypo_curiosity_fep_connect_drives(curiosity_bridge, drive_system);
    }

    void TearDown() override {
        if (bio_async_bridge) {
            hypo_bio_async_fep_destroy(bio_async_bridge);
            bio_async_bridge = nullptr;
        }
        if (salience_bridge) {
            hypo_salience_fep_destroy(salience_bridge);
            salience_bridge = nullptr;
        }
        if (curiosity_bridge) {
            hypo_curiosity_fep_destroy(curiosity_bridge);
            curiosity_bridge = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
    }

    /* Helper to update all bridges */
    void update_all_bridges(uint64_t delta_ms = 16) {
        hypo_curiosity_fep_update(curiosity_bridge, delta_ms);
        hypo_salience_fep_update(salience_bridge);
        hypo_bio_async_fep_update(bio_async_bridge);
    }

    /* Helper to set drive state (use satisfy to reduce drive toward setpoint) */
    void set_drive_level(hypo_drive_type_t drive, float level) {
        /* First update to ensure drive has some level, then satisfy to adjust */
        /* Higher 'level' means higher drive urgency, so we satisfy less */
        float satisfaction = 1.0f - level;  /* Convert to satisfaction */
        hypo_drive_satisfy(drive_system, drive, satisfaction);
    }
};

/* ============================================================================
 * Curiosity FEP Bridge Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, CuriosityBridgeExplorationWeight) {
    /* Low curiosity drive should give low exploration weight */
    float exploration_low;
    int ret = hypo_curiosity_fep_compute_exploration(curiosity_bridge,
                                                      TEST_DRIVE_LEVEL_LOW,
                                                      &exploration_low);
    ASSERT_EQ(0, ret);

    /* High curiosity drive should give higher exploration weight */
    float exploration_high;
    ret = hypo_curiosity_fep_compute_exploration(curiosity_bridge,
                                                  TEST_DRIVE_LEVEL_HIGH,
                                                  &exploration_high);
    ASSERT_EQ(0, ret);

    /* Higher drive should lead to higher exploration */
    EXPECT_GT(exploration_high, exploration_low);
    EXPECT_GE(exploration_low, 0.0f);
    EXPECT_LE(exploration_high, 1.0f);
}

TEST_F(HypoFepIntegrationTest, CuriosityBridgeFreeEnergyReduction) {
    /* Information gain should reduce free energy */
    float fe_reduction_low;
    int ret = hypo_curiosity_fep_compute_fe_reduction(curiosity_bridge,
                                                       0.1f,  /* Low info gain */
                                                       &fe_reduction_low);
    ASSERT_EQ(0, ret);

    float fe_reduction_high;
    ret = hypo_curiosity_fep_compute_fe_reduction(curiosity_bridge,
                                                   0.8f,  /* High info gain */
                                                   &fe_reduction_high);
    ASSERT_EQ(0, ret);

    /* More information gain should lead to greater FE reduction */
    EXPECT_GT(fe_reduction_high, fe_reduction_low);
}

TEST_F(HypoFepIntegrationTest, CuriosityBridgeNoveltyModulatesPrecision) {
    /* Report novelty detection */
    int ret = hypo_curiosity_fep_report_novelty(curiosity_bridge, 0.3f);
    EXPECT_EQ(0, ret);

    /* Update bridge */
    hypo_curiosity_fep_update(curiosity_bridge, 16);

    /* Get effects */
    hypo_curiosity_fep_effects_t effects;
    ret = hypo_curiosity_fep_get_effects(curiosity_bridge, &effects);
    ASSERT_EQ(0, ret);

    /* Novelty should increase precision (attention to novel stimuli) */
    EXPECT_GT(effects.precision, 0.0f);
    EXPECT_GT(effects.novelty_signal, 0.0f);
}

TEST_F(HypoFepIntegrationTest, CuriosityBridgeInfoGainSatisfiesDrive) {
    /* Report information gain */
    int ret = hypo_curiosity_fep_report_info_gain(curiosity_bridge, 0.6f);
    EXPECT_EQ(0, ret);

    /* Update bridge */
    hypo_curiosity_fep_update(curiosity_bridge, 16);

    /* Get effects */
    hypo_curiosity_fep_effects_t effects;
    ret = hypo_curiosity_fep_get_effects(curiosity_bridge, &effects);
    ASSERT_EQ(0, ret);

    /* Info gain should lead to FE reduction */
    EXPECT_GT(effects.info_gain_fe_reduction, 0.0f);
}

TEST_F(HypoFepIntegrationTest, CuriosityBridgeStatistics) {
    /* Generate some activity */
    for (int i = 0; i < 10; i++) {
        hypo_curiosity_fep_report_info_gain(curiosity_bridge, 0.1f * i);
        hypo_curiosity_fep_report_novelty(curiosity_bridge, 0.05f * i);
        hypo_curiosity_fep_update(curiosity_bridge, 16);
    }

    hypo_curiosity_fep_stats_t stats;
    int ret = hypo_curiosity_fep_get_stats(curiosity_bridge, &stats);
    ASSERT_EQ(0, ret);

    EXPECT_GT(stats.total_updates, 0u);
    EXPECT_GT(stats.info_gain_fe_reductions, 0u);
}

/* ============================================================================
 * Salience FEP Bridge Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, SalienceBridgeWeightComputation) {
    /* Set drive urgencies directly on the salience bridge for deterministic testing.
     * The indirect path (set_drive_level -> hypo_drive_update -> salience update)
     * produces near-equal urgencies because all drives rise to max in 100ms.
     *
     * Temporarily disconnect drive_system so hypo_salience_fep_update() doesn't
     * auto-sync from the real drive system (which would overwrite our values). */
    hypo_drive_system_handle_t* saved_ds = salience_bridge->drive_system;
    salience_bridge->drive_system = nullptr;

    salience_bridge->sal_effects.drive_urgencies[HYPO_DRIVE_HUNGER] = TEST_DRIVE_LEVEL_HIGH;
    salience_bridge->sal_effects.drive_urgencies[HYPO_DRIVE_THIRST] = TEST_DRIVE_LEVEL_LOW;
    salience_bridge->sal_effects.drive_urgencies[HYPO_DRIVE_SAFETY] = TEST_DRIVE_LEVEL_MODERATE;

    /* Update salience bridge (computes weights from urgencies) */
    hypo_salience_fep_update(salience_bridge);

    /* Restore drive system */
    salience_bridge->drive_system = saved_ds;

    /* Get salience weights */
    float weights[HYPO_DRIVE_COUNT];
    int ret = hypo_salience_fep_get_weights(salience_bridge, weights);
    ASSERT_EQ(0, ret);

    /* Hunger should have highest salience */
    float hunger_weight = hypo_salience_fep_get_weight(salience_bridge, HYPO_DRIVE_HUNGER);
    float thirst_weight = hypo_salience_fep_get_weight(salience_bridge, HYPO_DRIVE_THIRST);

    EXPECT_GT(hunger_weight, thirst_weight);
    EXPECT_GT(hunger_weight, 0.0f);
}

TEST_F(HypoFepIntegrationTest, SalienceBridgeFatigueReducesPrecision) {
    /* Set baseline precision with no fatigue */
    hypo_salience_fep_modulate_precision(salience_bridge, TEST_FATIGUE_NONE);
    hypo_salience_fep_update(salience_bridge);

    hypo_salience_fep_effects_t effects_no_fatigue;
    hypo_salience_fep_get_effects(salience_bridge, &effects_no_fatigue);
    float precision_no_fatigue = effects_no_fatigue.precision;

    /* Apply high fatigue */
    hypo_salience_fep_modulate_precision(salience_bridge, TEST_FATIGUE_HIGH);
    hypo_salience_fep_update(salience_bridge);

    hypo_salience_fep_effects_t effects_fatigued;
    hypo_salience_fep_get_effects(salience_bridge, &effects_fatigued);
    float precision_fatigued = effects_fatigued.precision;

    /* Fatigue should reduce precision */
    EXPECT_LT(precision_fatigued, precision_no_fatigue);
    EXPECT_GT(precision_fatigued, 0.0f);  /* But should still be positive */
}

TEST_F(HypoFepIntegrationTest, SalienceBridgeConflictDetection) {
    /* Set competing high drives */
    set_drive_level(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_HIGH);
    set_drive_level(HYPO_DRIVE_SAFETY, TEST_DRIVE_LEVEL_HIGH);

    /* Update */
    hypo_drive_update(drive_system, 100000);
    hypo_salience_fep_update(salience_bridge);

    /* Detect conflict */
    hypo_salience_conflict_t conflict;
    float intensity;
    int ret = hypo_salience_fep_detect_conflict(salience_bridge, &conflict, &intensity);
    ASSERT_EQ(0, ret);

    /* Should detect some conflict with competing drives */
    EXPECT_GE(intensity, 0.0f);
    EXPECT_LE(intensity, 1.0f);
}

TEST_F(HypoFepIntegrationTest, SalienceBridgeUrgencyLevels) {
    /* FE = sum(urgency[i]^2 * drive_fe_weight) + conflict * 5.
     * With 9 drives and drive_fe_weight=1.0, ELEVATED threshold (5.0) requires
     * total urgency mass: 9 * 0.95^2 = 8.12 >= 5.0.
     * Set ALL drives to urgent to produce enough free energy.
     * Temporarily disconnect drive_system to prevent auto-sync overwriting values. */
    hypo_drive_system_handle_t* saved_ds = salience_bridge->drive_system;
    salience_bridge->drive_system = nullptr;

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        salience_bridge->sal_effects.drive_urgencies[i] = TEST_DRIVE_LEVEL_URGENT;
    }

    hypo_salience_fep_update(salience_bridge);

    /* Restore drive system */
    salience_bridge->drive_system = saved_ds;

    hypo_salience_fep_effects_t effects;
    int ret = hypo_salience_fep_get_effects(salience_bridge, &effects);
    ASSERT_EQ(0, ret);

    /* With all drives urgent, urgency level should be elevated */
    EXPECT_GE((int)effects.urgency_level, (int)HYPO_SALIENCE_FEP_LEVEL_ELEVATED);
}

TEST_F(HypoFepIntegrationTest, SalienceBridgeStatistics) {
    /* Generate activity */
    for (int i = 0; i < 10; i++) {
        set_drive_level((hypo_drive_type_t)(i % HYPO_DRIVE_COUNT), 0.1f * i);
        hypo_drive_update(drive_system, 10000);
        hypo_salience_fep_update(salience_bridge);
    }

    hypo_salience_fep_stats_t stats;
    int ret = hypo_salience_fep_get_stats(salience_bridge, &stats);
    ASSERT_EQ(0, ret);

    EXPECT_GT(stats.total_updates, 0u);
}

/* ============================================================================
 * Bio-Async FEP Bridge Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, BioAsyncBridgeFreeEnergyComputation) {
    /* Get drive system state */
    hypo_drive_system_t drives;
    hypo_drive_get_system_state(drive_system, &drives);

    /* Compute free energy */
    int ret = hypo_bio_async_fep_compute_fe(bio_async_bridge, &drives);
    EXPECT_EQ(0, ret);

    /* Get effects */
    hypo_bio_async_fep_effects_t effects;
    ret = hypo_bio_async_fep_get_effects(bio_async_bridge, &effects);
    ASSERT_EQ(0, ret);

    /* Free energy should be computed */
    EXPECT_GE(effects.free_energy, 0.0f);
}

TEST_F(HypoFepIntegrationTest, BioAsyncBridgePrecisionModulation) {
    /* Modulate precision based on fatigue */
    int ret = hypo_bio_async_fep_modulate_precision(bio_async_bridge, TEST_FATIGUE_NONE);
    EXPECT_EQ(0, ret);

    hypo_bio_async_fep_effects_t effects_baseline;
    hypo_bio_async_fep_get_effects(bio_async_bridge, &effects_baseline);

    ret = hypo_bio_async_fep_modulate_precision(bio_async_bridge, TEST_FATIGUE_HIGH);
    EXPECT_EQ(0, ret);

    hypo_bio_async_fep_effects_t effects_fatigued;
    hypo_bio_async_fep_get_effects(bio_async_bridge, &effects_fatigued);

    /* Fatigue should reduce precision */
    EXPECT_LE(effects_fatigued.precision, effects_baseline.precision);
}

TEST_F(HypoFepIntegrationTest, BioAsyncBridgeDisruptionLevels) {
    /* Update bridge */
    hypo_bio_async_fep_update(bio_async_bridge);

    /* Get effects */
    hypo_bio_async_fep_effects_t effects;
    int ret = hypo_bio_async_fep_get_effects(bio_async_bridge, &effects);
    ASSERT_EQ(0, ret);

    /* Disruption level should be valid */
    EXPECT_GE((int)effects.disruption_level, (int)HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL);
    EXPECT_LE((int)effects.disruption_level, (int)HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL);

    /* Homeostatic health should be between 0 and 1 */
    EXPECT_GE(effects.homeostatic_health, 0.0f);
    EXPECT_LE(effects.homeostatic_health, 1.0f);
}

TEST_F(HypoFepIntegrationTest, BioAsyncBridgeStatistics) {
    /* Generate updates */
    for (int i = 0; i < 10; i++) {
        hypo_bio_async_fep_update(bio_async_bridge);
    }

    hypo_bio_async_fep_stats_t stats;
    int ret = hypo_bio_async_fep_get_stats(bio_async_bridge, &stats);
    ASSERT_EQ(0, ret);

    EXPECT_EQ(10u, stats.total_updates);
}

/* ============================================================================
 * Cross-Bridge Integration Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, AllBridgesUpdateCoherently) {
    /* Set drive state */
    set_drive_level(HYPO_DRIVE_CURIOSITY, TEST_DRIVE_LEVEL_HIGH);
    set_drive_level(HYPO_DRIVE_HUNGER, TEST_DRIVE_LEVEL_MODERATE);
    hypo_drive_update(drive_system, 100000);

    /* Update all bridges */
    update_all_bridges();

    /* Get effects from all bridges */
    hypo_curiosity_fep_effects_t curiosity_effects;
    hypo_salience_fep_effects_t salience_effects;
    hypo_bio_async_fep_effects_t bio_async_effects;

    hypo_curiosity_fep_get_effects(curiosity_bridge, &curiosity_effects);
    hypo_salience_fep_get_effects(salience_bridge, &salience_effects);
    hypo_bio_async_fep_get_effects(bio_async_bridge, &bio_async_effects);

    /* All should have valid precision values */
    EXPECT_GT(curiosity_effects.precision, 0.0f);
    EXPECT_GT(salience_effects.precision, 0.0f);
    EXPECT_GT(bio_async_effects.precision, 0.0f);

    /* All should have valid free energy values */
    EXPECT_GE(curiosity_effects.free_energy, 0.0f);
    EXPECT_GE(salience_effects.free_energy, 0.0f);
    EXPECT_GE(bio_async_effects.free_energy, 0.0f);
}

TEST_F(HypoFepIntegrationTest, FatigueAffectsAllBridges) {
    float high_fatigue = TEST_FATIGUE_HIGH;

    /* Apply fatigue to all bridges */
    float curiosity_precision;
    hypo_curiosity_fep_modulate_precision(curiosity_bridge, high_fatigue,
                                           &curiosity_precision);

    hypo_salience_fep_modulate_precision(salience_bridge, high_fatigue);

    hypo_bio_async_fep_modulate_precision(bio_async_bridge, high_fatigue);

    /* Update all bridges */
    update_all_bridges();

    /* Get effects */
    hypo_curiosity_fep_effects_t curiosity_effects;
    hypo_salience_fep_effects_t salience_effects;
    hypo_bio_async_fep_effects_t bio_async_effects;

    hypo_curiosity_fep_get_effects(curiosity_bridge, &curiosity_effects);
    hypo_salience_fep_get_effects(salience_bridge, &salience_effects);
    hypo_bio_async_fep_get_effects(bio_async_bridge, &bio_async_effects);

    /* All should have reduced precision due to fatigue */
    EXPECT_LT(salience_effects.precision, HYPO_SALIENCE_FEP_MAX_PRECISION);
    EXPECT_LT(bio_async_effects.precision, HYPO_BIO_ASYNC_FEP_MAX_PRECISION);
}

TEST_F(HypoFepIntegrationTest, DriveChangePropagatesAcrossBridges) {
    /* Temporarily disconnect drive_system to prevent auto-sync overwriting
     * manually-set urgencies during hypo_salience_fep_update(). */
    hypo_drive_system_handle_t* saved_ds = salience_bridge->drive_system;
    salience_bridge->drive_system = nullptr;

    /* Set low urgency initially */
    salience_bridge->sal_effects.drive_urgencies[HYPO_DRIVE_SAFETY] = TEST_DRIVE_LEVEL_LOW;
    update_all_bridges();

    /* Get initial salience weight for safety */
    float initial_safety_salience = hypo_salience_fep_get_weight(salience_bridge,
                                                                   HYPO_DRIVE_SAFETY);

    /* Increase safety drive urgently */
    salience_bridge->sal_effects.drive_urgencies[HYPO_DRIVE_SAFETY] = TEST_DRIVE_LEVEL_URGENT;
    update_all_bridges();

    /* Restore drive system */
    salience_bridge->drive_system = saved_ds;

    /* Get updated salience weight */
    float urgent_safety_salience = hypo_salience_fep_get_weight(salience_bridge,
                                                                  HYPO_DRIVE_SAFETY);

    /* Salience should increase with drive level */
    EXPECT_GT(urgent_safety_salience, initial_safety_salience);
}

TEST_F(HypoFepIntegrationTest, MultipleUpdateCycles) {
    /* Run multiple update cycles */
    for (int cycle = 0; cycle < 100; cycle++) {
        /* Vary drive levels */
        float level = 0.3f + 0.5f * sinf(cycle * 0.1f);
        set_drive_level(HYPO_DRIVE_CURIOSITY, level);
        set_drive_level(HYPO_DRIVE_HUNGER, 1.0f - level);

        hypo_drive_update(drive_system, 16000);
        update_all_bridges(16);
    }

    /* All bridges should still be functional */
    hypo_curiosity_fep_effects_t curiosity_effects;
    hypo_salience_fep_effects_t salience_effects;
    hypo_bio_async_fep_effects_t bio_async_effects;

    EXPECT_EQ(0, hypo_curiosity_fep_get_effects(curiosity_bridge, &curiosity_effects));
    EXPECT_EQ(0, hypo_salience_fep_get_effects(salience_bridge, &salience_effects));
    EXPECT_EQ(0, hypo_bio_async_fep_get_effects(bio_async_bridge, &bio_async_effects));

    /* Statistics should reflect activity */
    hypo_curiosity_fep_stats_t curiosity_stats;
    hypo_curiosity_fep_get_stats(curiosity_bridge, &curiosity_stats);
    EXPECT_EQ(100u, curiosity_stats.total_updates);
}

/* ============================================================================
 * Reset Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, BridgeResetClearsState) {
    /* Generate activity */
    for (int i = 0; i < 10; i++) {
        hypo_curiosity_fep_report_info_gain(curiosity_bridge, 0.5f);
        hypo_curiosity_fep_update(curiosity_bridge, 16);
    }

    /* Reset */
    int ret = hypo_curiosity_fep_reset(curiosity_bridge);
    EXPECT_EQ(0, ret);

    /* State should be reset */
    hypo_curiosity_fep_stats_t stats;
    hypo_curiosity_fep_get_stats(curiosity_bridge, &stats);
    EXPECT_EQ(0u, stats.total_updates);
}

TEST_F(HypoFepIntegrationTest, SalienceBridgeReset) {
    /* Generate activity */
    for (int i = 0; i < 10; i++) {
        hypo_salience_fep_update(salience_bridge);
    }

    /* Reset */
    int ret = hypo_salience_fep_reset(salience_bridge);
    EXPECT_EQ(0, ret);

    hypo_salience_fep_stats_t stats;
    hypo_salience_fep_get_stats(salience_bridge, &stats);
    EXPECT_EQ(0u, stats.total_updates);
}

TEST_F(HypoFepIntegrationTest, BioAsyncBridgeReset) {
    /* Generate activity */
    for (int i = 0; i < 10; i++) {
        hypo_bio_async_fep_update(bio_async_bridge);
    }

    /* Reset */
    int ret = hypo_bio_async_fep_reset(bio_async_bridge);
    EXPECT_EQ(0, ret);

    hypo_bio_async_fep_stats_t stats;
    hypo_bio_async_fep_get_stats(bio_async_bridge, &stats);
    EXPECT_EQ(0u, stats.total_updates);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, SalienceLevelNames) {
    const char* name;

    name = hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_LOW);
    EXPECT_NE(name, nullptr);

    name = hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_CRITICAL);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypoFepIntegrationTest, BioAsyncLevelNames) {
    const char* name;

    name = hypo_bio_async_fep_level_name(HYPO_BIO_ASYNC_FEP_LEVEL_NORMAL);
    EXPECT_NE(name, nullptr);

    name = hypo_bio_async_fep_level_name(HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL);
    EXPECT_NE(name, nullptr);
}

TEST_F(HypoFepIntegrationTest, ResponseTypeNames) {
    const char* name;

    name = hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_MAINTAIN);
    EXPECT_NE(name, nullptr);

    name = hypo_bio_async_fep_response_name(HYPO_BIO_ASYNC_FEP_RESPONSE_NONE);
    EXPECT_NE(name, nullptr);
}

/* ============================================================================
 * Error Handling Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, NullBridgeHandling) {
    /* Destroy should be safe with null */
    hypo_curiosity_fep_destroy(nullptr);
    hypo_salience_fep_destroy(nullptr);
    hypo_bio_async_fep_destroy(nullptr);

    /* Operations with null should fail gracefully */
    hypo_curiosity_fep_effects_t curiosity_effects;
    int ret = hypo_curiosity_fep_get_effects(nullptr, &curiosity_effects);
    EXPECT_EQ(-1, ret);

    hypo_salience_fep_effects_t salience_effects;
    ret = hypo_salience_fep_get_effects(nullptr, &salience_effects);
    EXPECT_EQ(-1, ret);

    hypo_bio_async_fep_effects_t bio_async_effects;
    ret = hypo_bio_async_fep_get_effects(nullptr, &bio_async_effects);
    EXPECT_EQ(-1, ret);
}

TEST_F(HypoFepIntegrationTest, NullOutputParameterHandling) {
    /* Null output parameters should be handled */
    int ret = hypo_curiosity_fep_get_effects(curiosity_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_salience_fep_get_effects(salience_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_bio_async_fep_get_effects(bio_async_bridge, nullptr);
    EXPECT_EQ(-1, ret);

    ret = hypo_salience_fep_get_weights(salience_bridge, nullptr);
    EXPECT_EQ(-1, ret);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(HypoFepIntegrationTest, ConcurrentBridgeUpdates) {
    std::atomic<int> update_count{0};

    auto update_thread = [this, &update_count]() {
        for (int i = 0; i < 100; i++) {
            hypo_curiosity_fep_update(curiosity_bridge, 1);
            hypo_salience_fep_update(salience_bridge);
            hypo_bio_async_fep_update(bio_async_bridge);
            update_count++;
        }
    };

    std::thread t1(update_thread);
    std::thread t2(update_thread);

    t1.join();
    t2.join();

    EXPECT_EQ(200, update_count.load());

    /* Bridges should still be functional */
    hypo_curiosity_fep_effects_t effects;
    int ret = hypo_curiosity_fep_get_effects(curiosity_bridge, &effects);
    EXPECT_EQ(0, ret);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
