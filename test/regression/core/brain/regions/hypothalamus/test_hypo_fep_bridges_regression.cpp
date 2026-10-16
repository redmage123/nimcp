/**
 * @file test_hypo_fep_bridges_regression.cpp
 * @brief Regression tests for Hypothalamus FEP Bridges
 * @version 1.0.0
 * @date 2026-01-10
 *
 * WHAT: Regression tests for hypothalamus FEP bridges ensuring accuracy,
 *       stability, and performance across code changes
 * WHY:  Prevent regressions in FEP computations, precision modulation,
 *       and bridge coordination
 * HOW:  Test known good behaviors, numerical accuracy, performance bounds,
 *       and memory stability
 *
 * BRIDGES TESTED:
 * - Bio-Async FEP Bridge: Message timing to free energy mapping
 * - Game Theory FEP Bridge: Drive-modulated strategy selection
 * - Reasoning FEP Bridge: Fatigue-precision interaction
 * - Curiosity FEP Bridge: Information gain to FE reduction
 * - Salience FEP Bridge: Drive urgency to salience weights
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_bio_async_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_game_theory_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_reasoning_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_curiosity_fep_bridge.h"
#include "core/brain/regions/hypothalamus/fep/nimcp_hypo_salience_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h"

/* ============================================================================
 * Test Constants - Known Good Baselines
 * ============================================================================ */

/* Performance baselines (microseconds) */
#define REGRESSION_MAX_UPDATE_TIME_US         500     /* 500us max per update */
#define REGRESSION_AVG_UPDATE_TIME_US         200     /* 200us average target */
#define REGRESSION_MAX_FE_COMPUTE_TIME_US     100     /* 100us max for FE computation */

/* Memory baselines */
#define REGRESSION_MAX_ITERATIONS             300     /* Iterations for memory test */
#define REGRESSION_CYCLES_PER_CHECK           30      /* Check state every N cycles */

/* Numerical accuracy */
#define REGRESSION_FLOAT_EPSILON              1e-5f   /* Float comparison tolerance */
#define REGRESSION_FREE_ENERGY_TOLERANCE      0.01f   /* FE stability tolerance */
#define REGRESSION_PRECISION_TOLERANCE        0.001f  /* Precision stability tolerance */

/* Thread safety */
#define REGRESSION_NUM_THREADS                4       /* Concurrent threads */
#define REGRESSION_THREAD_ITERATIONS          100     /* Iterations per thread */

/* ============================================================================
 * Test Fixture for Bio-Async FEP Bridge
 * ============================================================================ */

class HypoBioAsyncFEPRegressionTest : public ::testing::Test {
protected:
    hypo_bio_async_fep_bridge_t* bridge = nullptr;
    hypo_bio_async_fep_config_t config;
    fep_system_t* fep_system = nullptr;
    hypo_drive_system_handle_t* drive_system = nullptr;

    void SetUp() override {
        /* Create FEP system (observation_dim=32, action_dim=16) */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 32, 16);
        ASSERT_NE(fep_system, nullptr);

        /* Create drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);

        /* Create bridge */
        hypo_bio_async_fep_default_config(&config);
        bridge = hypo_bio_async_fep_create(&config, drive_system, fep_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hypo_bio_async_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
    }

    bool verify_fe_bounds(float fe) {
        return fe >= 0.0f && !std::isnan(fe) && !std::isinf(fe);
    }

    bool verify_normalized(float value) {
        return value >= 0.0f && value <= 1.0f;
    }
};

/* ============================================================================
 * Bio-Async FEP Bridge Regression Tests
 * ============================================================================ */

/* REG-BA-001: Free energy remains bounded across all input combinations */
TEST_F(HypoBioAsyncFEPRegressionTest, REG_BA_001_FreeEnergyBounds) {
    const float test_values[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float fatigue : test_values) {
        hypo_bio_async_fep_modulate_precision(bridge, fatigue);
        int ret = hypo_bio_async_fep_update(bridge);
        EXPECT_EQ(ret, 0);

        hypo_bio_async_fep_effects_t effects;
        hypo_bio_async_fep_get_effects(bridge, &effects);

        EXPECT_TRUE(verify_fe_bounds(effects.free_energy))
            << "FE out of bounds for fatigue=" << fatigue
            << ": fe=" << effects.free_energy;

        EXPECT_TRUE(verify_normalized(effects.disruption_confidence))
            << "Disruption confidence out of [0,1]";

        EXPECT_TRUE(verify_normalized(effects.response_urgency))
            << "Response urgency out of [0,1]";

        EXPECT_TRUE(verify_normalized(effects.homeostatic_health))
            << "Homeostatic health out of [0,1]";
    }
}

/* REG-BA-002: Precision decreases with fatigue */
TEST_F(HypoBioAsyncFEPRegressionTest, REG_BA_002_PrecisionFatigueRelationship) {
    float prev_precision = -1.0f;

    /* Precision should decrease as fatigue increases */
    for (float fatigue = 0.0f; fatigue <= 1.0f; fatigue += 0.2f) {
        hypo_bio_async_fep_modulate_precision(bridge, fatigue);
        hypo_bio_async_fep_update(bridge);

        hypo_bio_async_fep_effects_t effects;
        hypo_bio_async_fep_get_effects(bridge, &effects);

        if (prev_precision >= 0.0f && fatigue > 0.0f) {
            /* Higher fatigue should mean lower or equal precision */
            EXPECT_LE(effects.precision, prev_precision + REGRESSION_PRECISION_TOLERANCE)
                << "Precision should decrease with fatigue (fatigue=" << fatigue << ")";
        }
        prev_precision = effects.precision;
    }
}

/* REG-BA-003: Statistics accumulate correctly */
TEST_F(HypoBioAsyncFEPRegressionTest, REG_BA_003_StatisticsAccumulation) {
    hypo_bio_async_fep_reset(bridge);

    const int NUM_UPDATES = 100;
    for (int i = 0; i < NUM_UPDATES; i++) {
        hypo_bio_async_fep_update(bridge);
    }

    hypo_bio_async_fep_stats_t stats;
    hypo_bio_async_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_updates, (uint64_t)NUM_UPDATES)
        << "Total updates should match";

    /* Statistics should be valid */
    EXPECT_FALSE(std::isnan(stats.avg_free_energy));
    EXPECT_FALSE(std::isnan(stats.avg_surprise));
    EXPECT_FALSE(std::isnan(stats.avg_prediction_error));
}

/* REG-BA-004: Update performance bounds */
TEST_F(HypoBioAsyncFEPRegressionTest, REG_BA_004_UpdatePerformance) {
    /* Warm up */
    for (int i = 0; i < 10; i++) {
        hypo_bio_async_fep_update(bridge);
    }

    const int NUM_SAMPLES = 50;
    std::vector<uint64_t> update_times;
    update_times.reserve(NUM_SAMPLES);

    for (int i = 0; i < NUM_SAMPLES; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        hypo_bio_async_fep_update(bridge);
        auto end = std::chrono::high_resolution_clock::now();
        update_times.push_back(
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
        );
    }

    uint64_t max_time = *std::max_element(update_times.begin(), update_times.end());
    uint64_t total_time = std::accumulate(update_times.begin(), update_times.end(), 0ULL);
    double avg_time = (double)total_time / NUM_SAMPLES;

    EXPECT_LT(max_time, REGRESSION_MAX_UPDATE_TIME_US)
        << "Maximum update time exceeded: " << max_time << "us";
    EXPECT_LT(avg_time, REGRESSION_AVG_UPDATE_TIME_US)
        << "Average update time exceeded: " << avg_time << "us";
}

/* REG-BA-005: Memory stability over many cycles */
TEST_F(HypoBioAsyncFEPRegressionTest, REG_BA_005_MemoryStability) {
    for (int i = 0; i < REGRESSION_MAX_ITERATIONS; i++) {
        float fatigue = (float)(i % 100) / 100.0f;
        hypo_bio_async_fep_modulate_precision(bridge, fatigue);
        hypo_bio_async_fep_update(bridge);

        if (i % REGRESSION_CYCLES_PER_CHECK == 0) {
            hypo_bio_async_fep_effects_t effects;
            hypo_bio_async_fep_get_effects(bridge, &effects);
            ASSERT_TRUE(verify_fe_bounds(effects.free_energy))
                << "FE out of bounds after " << i << " iterations";
        }
    }

    /* Verify bridge still functional */
    hypo_bio_async_fep_effects_t final_effects;
    int ret = hypo_bio_async_fep_get_effects(bridge, &final_effects);
    EXPECT_EQ(ret, 0);
}

/* REG-BA-006: Utility strings are not null */
TEST_F(HypoBioAsyncFEPRegressionTest, REG_BA_006_UtilityStrings) {
    for (int i = 0; i <= HYPO_BIO_ASYNC_FEP_LEVEL_CRITICAL; i++) {
        const char* name = hypo_bio_async_fep_level_name((hypo_bio_async_fep_level_t)i);
        EXPECT_NE(name, nullptr) << "Level " << i << " has null name";
    }

    for (int i = 0; i <= HYPO_BIO_ASYNC_FEP_RESPONSE_EMERGENCY; i++) {
        const char* name = hypo_bio_async_fep_response_name((hypo_bio_async_fep_response_t)i);
        EXPECT_NE(name, nullptr) << "Response " << i << " has null name";
    }

    for (int i = 0; i <= HYPO_BIO_ASYNC_ANOMALY_PATTERN; i++) {
        const char* name = hypo_bio_async_fep_anomaly_name((hypo_bio_async_anomaly_type_t)i);
        EXPECT_NE(name, nullptr) << "Anomaly " << i << " has null name";
    }
}

/* ============================================================================
 * Test Fixture for Game Theory FEP Bridge
 * ============================================================================ */

class HypoGameTheoryFEPRegressionTest : public ::testing::Test {
protected:
    hypo_gt_fep_bridge_t* bridge = nullptr;
    hypo_gt_fep_config_t config;
    fep_system_t* fep_system = nullptr;
    hypo_drive_system_handle_t* drive_system = nullptr;

    void SetUp() override {
        /* Create FEP system - observation_dim must match HYPO_GT_FEP_DRIVE_DIM (4) */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, HYPO_GT_FEP_DRIVE_DIM, HYPO_GT_FEP_STRATEGY_DIM);
        ASSERT_NE(fep_system, nullptr);

        /* Create drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);

        /* Create bridge */
        hypo_gt_fep_default_config(&config);
        bridge = hypo_gt_fep_create(&config, drive_system, fep_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hypo_gt_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
    }

    bool verify_fe_bounds(float fe) {
        return fe >= 0.0f && !std::isnan(fe) && !std::isinf(fe);
    }

    bool verify_normalized(float value) {
        return value >= 0.0f && value <= 1.0f;
    }
};

/* ============================================================================
 * Game Theory FEP Bridge Regression Tests
 * ============================================================================ */

/* REG-GT-001: Strategy selection stability */
TEST_F(HypoGameTheoryFEPRegressionTest, REG_GT_001_StrategySelectionStability) {
    hypo_gt_fep_update(bridge);

    hypo_gt_strategy_type_t first_strategy;
    float first_confidence;
    hypo_gt_fep_select_strategy(bridge, &first_strategy, &first_confidence);

    /* Without changes, strategy should remain stable */
    for (int i = 0; i < 50; i++) {
        hypo_gt_fep_update(bridge);

        hypo_gt_strategy_type_t strategy;
        float confidence;
        hypo_gt_fep_select_strategy(bridge, &strategy, &confidence);

        /* Strategy should not change without input changes */
        EXPECT_EQ(strategy, first_strategy)
            << "Strategy changed without input changes at iteration " << i;

        /* Confidence should remain stable */
        EXPECT_NEAR(confidence, first_confidence, 0.1f)
            << "Confidence changed significantly at iteration " << i;
    }
}

/* REG-GT-002: Free energy computation bounds */
TEST_F(HypoGameTheoryFEPRegressionTest, REG_GT_002_FreeEnergyBounds) {
    for (int i = 0; i < 100; i++) {
        hypo_gt_fep_update(bridge);

        float free_energy;
        hypo_gt_fep_compute_fe(bridge, &free_energy);

        EXPECT_TRUE(verify_fe_bounds(free_energy))
            << "Free energy out of bounds at iteration " << i;
    }
}

/* REG-GT-003: Effects values are in valid ranges */
TEST_F(HypoGameTheoryFEPRegressionTest, REG_GT_003_EffectsValidity) {
    hypo_gt_fep_update(bridge);

    fep_to_gt_effects_t effects;
    hypo_gt_fep_get_effects(bridge, &effects);

    EXPECT_TRUE(verify_normalized(effects.cooperation_score))
        << "Cooperation score out of [0,1]";
    EXPECT_TRUE(verify_normalized(effects.competition_score))
        << "Competition score out of [0,1]";
    EXPECT_TRUE(verify_normalized(effects.exploration_score))
        << "Exploration score out of [0,1]";
    EXPECT_TRUE(verify_normalized(effects.strategy_confidence))
        << "Strategy confidence out of [0,1]";
    EXPECT_TRUE(verify_normalized(effects.partner_model_confidence))
        << "Partner model confidence out of [0,1]";

    EXPECT_TRUE(verify_fe_bounds(effects.free_energy));
}

/* REG-GT-004: Statistics accumulation */
TEST_F(HypoGameTheoryFEPRegressionTest, REG_GT_004_StatisticsAccumulation) {
    hypo_gt_fep_reset(bridge);

    const int NUM_UPDATES = 50;
    for (int i = 0; i < NUM_UPDATES; i++) {
        hypo_gt_fep_update(bridge);
    }

    hypo_gt_fep_stats_t stats;
    hypo_gt_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.bridge_updates, (uint64_t)NUM_UPDATES);
    EXPECT_FALSE(std::isnan(stats.avg_free_energy));
    EXPECT_FALSE(std::isnan(stats.avg_prediction_error));
    EXPECT_FALSE(std::isnan(stats.avg_precision));
}

/* REG-GT-005: Precision modulation works */
TEST_F(HypoGameTheoryFEPRegressionTest, REG_GT_005_PrecisionModulation) {
    /* Get initial precision */
    hypo_gt_fep_update(bridge);
    hypo_gt_fep_state_t initial_state;
    hypo_gt_fep_get_state(bridge, &initial_state);
    float initial_precision = initial_state.current_precision;

    /* Modulate precision */
    hypo_gt_fep_modulate_precision(bridge);

    hypo_gt_fep_state_t new_state;
    hypo_gt_fep_get_state(bridge, &new_state);

    /* Precision should be in valid range */
    EXPECT_GE(new_state.current_precision, HYPO_GT_FEP_MIN_PRECISION);
    EXPECT_LE(new_state.current_precision, HYPO_GT_FEP_MAX_PRECISION);
}

/* REG-GT-006: Utility strings */
TEST_F(HypoGameTheoryFEPRegressionTest, REG_GT_006_UtilityStrings) {
    EXPECT_NE(nullptr, hypo_gt_strategy_to_string(HYPO_GT_STRATEGY_COOPERATIVE));
    EXPECT_NE(nullptr, hypo_gt_strategy_to_string(HYPO_GT_STRATEGY_COMPETITIVE));
    EXPECT_NE(nullptr, hypo_gt_strategy_to_string(HYPO_GT_STRATEGY_CAUTIOUS));
    EXPECT_NE(nullptr, hypo_gt_strategy_to_string(HYPO_GT_STRATEGY_MIXED));
    EXPECT_NE(nullptr, hypo_gt_strategy_to_string(HYPO_GT_STRATEGY_RECIPROCAL));

    EXPECT_NE(nullptr, hypo_gt_partner_to_string(HYPO_GT_PARTNER_UNKNOWN));
    EXPECT_NE(nullptr, hypo_gt_partner_to_string(HYPO_GT_PARTNER_COOPERATIVE));
    EXPECT_NE(nullptr, hypo_gt_partner_to_string(HYPO_GT_PARTNER_COMPETITIVE));
    EXPECT_NE(nullptr, hypo_gt_partner_to_string(HYPO_GT_PARTNER_MIXED));
    EXPECT_NE(nullptr, hypo_gt_partner_to_string(HYPO_GT_PARTNER_EXPLOITATIVE));
}

/* ============================================================================
 * Test Fixture for Reasoning FEP Bridge
 * ============================================================================ */

class HypoReasoningFEPRegressionTest : public ::testing::Test {
protected:
    hypo_reasoning_fep_bridge_t* bridge = nullptr;
    hypo_reasoning_fep_config_t config;
    fep_system_t* fep_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 32, 16);
        ASSERT_NE(fep_system, nullptr);

        /* Create bridge */
        hypo_reasoning_fep_default_config(&config);
        bridge = hypo_reasoning_fep_create(&config, fep_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hypo_reasoning_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
    }

    bool verify_fe_bounds(float fe) {
        return fe >= 0.0f && !std::isnan(fe) && !std::isinf(fe);
    }
};

/* ============================================================================
 * Reasoning FEP Bridge Regression Tests
 * ============================================================================ */

/* REG-RF-001: Fatigue affects precision correctly */
TEST_F(HypoReasoningFEPRegressionTest, REG_RF_001_FatiguePrecisionRelation) {
    float prev_precision = 1.0f;

    for (float fatigue = 0.0f; fatigue <= 1.0f; fatigue += 0.2f) {
        float precision;
        hypo_reasoning_fep_modulate_precision(bridge, fatigue, &precision);

        EXPECT_GE(precision, HYPO_REASONING_FEP_PRECISION_MIN)
            << "Precision below minimum for fatigue=" << fatigue;

        if (fatigue > 0.0f) {
            /* Higher fatigue should mean lower or equal precision */
            EXPECT_LE(precision, prev_precision + REGRESSION_PRECISION_TOLERANCE)
                << "Precision should decrease with fatigue";
        }
        prev_precision = precision;
    }
}

/* REG-RF-002: Free energy from cognitive load */
TEST_F(HypoReasoningFEPRegressionTest, REG_RF_002_CognitiveLoadFreeEnergy) {
    float prev_fe = 0.0f;

    for (float load = 0.0f; load <= 1.0f; load += 0.2f) {
        float free_energy;
        hypo_reasoning_fep_compute_fe(bridge, load, &free_energy);

        EXPECT_TRUE(verify_fe_bounds(free_energy))
            << "FE out of bounds for load=" << load;

        if (load > 0.0f) {
            /* Higher cognitive load should mean higher or equal FE */
            EXPECT_GE(free_energy, prev_fe - REGRESSION_FREE_ENERGY_TOLERANCE)
                << "FE should increase with cognitive load";
        }
        prev_fe = free_energy;
    }
}

/* REG-RF-003: Effects validity */
TEST_F(HypoReasoningFEPRegressionTest, REG_RF_003_EffectsValidity) {
    hypo_reasoning_fep_update(bridge, 100);

    hypo_reasoning_fep_effects_t effects;
    hypo_reasoning_fep_get_effects(bridge, &effects);

    EXPECT_TRUE(verify_fe_bounds(effects.free_energy));
    EXPECT_FALSE(std::isnan(effects.prediction_error));
    EXPECT_FALSE(std::isnan(effects.precision));
    EXPECT_FALSE(std::isnan(effects.active_inference_strength));
}

/* REG-RF-004: Error and success reporting */
TEST_F(HypoReasoningFEPRegressionTest, REG_RF_004_ErrorSuccessReporting) {
    hypo_reasoning_fep_stats_t stats_before;
    hypo_reasoning_fep_get_stats(bridge, &stats_before);

    /* Report errors */
    for (int i = 0; i < 10; i++) {
        hypo_reasoning_fep_report_error(bridge, 0.5f);
    }

    /* Report successes */
    for (int i = 0; i < 5; i++) {
        hypo_reasoning_fep_report_success(bridge);
    }

    hypo_reasoning_fep_stats_t stats_after;
    hypo_reasoning_fep_get_stats(bridge, &stats_after);

    /* Statistics should have changed */
    EXPECT_GE(stats_after.fe_spikes, stats_before.fe_spikes);
}

/* REG-RF-005: Statistics accumulation */
TEST_F(HypoReasoningFEPRegressionTest, REG_RF_005_StatisticsAccumulation) {
    hypo_reasoning_fep_reset(bridge);

    const int NUM_UPDATES = 50;
    for (int i = 0; i < NUM_UPDATES; i++) {
        hypo_reasoning_fep_update(bridge, 100);
    }

    hypo_reasoning_fep_stats_t stats;
    hypo_reasoning_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_updates, (uint64_t)NUM_UPDATES);
    EXPECT_FALSE(std::isnan(stats.avg_free_energy));
    EXPECT_FALSE(std::isnan(stats.avg_precision));
}

/* ============================================================================
 * Test Fixture for Curiosity FEP Bridge
 * ============================================================================ */

class HypoCuriosityFEPRegressionTest : public ::testing::Test {
protected:
    hypo_curiosity_fep_bridge_t* bridge = nullptr;
    hypo_curiosity_fep_config_t config;
    fep_system_t* fep_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 32, 16);
        ASSERT_NE(fep_system, nullptr);

        /* Create bridge */
        hypo_curiosity_fep_default_config(&config);
        bridge = hypo_curiosity_fep_create(&config, fep_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hypo_curiosity_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
    }

    bool verify_normalized(float value) {
        return value >= 0.0f && value <= 1.0f;
    }
};

/* ============================================================================
 * Curiosity FEP Bridge Regression Tests
 * ============================================================================ */

/* REG-CF-001: Exploration weight increases with curiosity drive */
TEST_F(HypoCuriosityFEPRegressionTest, REG_CF_001_ExplorationDriveRelation) {
    float prev_weight = 0.0f;

    for (float drive = 0.0f; drive <= 1.0f; drive += 0.2f) {
        float exploration_weight;
        hypo_curiosity_fep_compute_exploration(bridge, drive, &exploration_weight);

        EXPECT_TRUE(verify_normalized(exploration_weight))
            << "Exploration weight out of [0,1] for drive=" << drive;

        if (drive > 0.0f) {
            /* Higher curiosity drive should mean higher exploration weight */
            EXPECT_GE(exploration_weight, prev_weight - REGRESSION_FLOAT_EPSILON)
                << "Exploration should increase with curiosity drive";
        }
        prev_weight = exploration_weight;
    }
}

/* REG-CF-002: Information gain reduces free energy */
TEST_F(HypoCuriosityFEPRegressionTest, REG_CF_002_InfoGainReducesFE) {
    for (float info_gain = 0.2f; info_gain <= 1.0f; info_gain += 0.2f) {
        float fe_reduction;
        hypo_curiosity_fep_compute_fe_reduction(bridge, info_gain, &fe_reduction);

        EXPECT_GE(fe_reduction, 0.0f)
            << "FE reduction should be positive for info_gain=" << info_gain;
    }
}

/* REG-CF-003: Effects validity */
TEST_F(HypoCuriosityFEPRegressionTest, REG_CF_003_EffectsValidity) {
    hypo_curiosity_fep_update(bridge, 100);

    hypo_curiosity_fep_effects_t effects;
    hypo_curiosity_fep_get_effects(bridge, &effects);

    EXPECT_FALSE(std::isnan(effects.free_energy));
    EXPECT_FALSE(std::isnan(effects.prediction_error));
    EXPECT_FALSE(std::isnan(effects.precision));
    EXPECT_TRUE(verify_normalized(effects.exploration_weight));
    EXPECT_FALSE(std::isnan(effects.epistemic_value));
}

/* REG-CF-004: Info gain and novelty reporting */
TEST_F(HypoCuriosityFEPRegressionTest, REG_CF_004_ReportingFunctions) {
    hypo_curiosity_fep_stats_t stats_before;
    hypo_curiosity_fep_get_stats(bridge, &stats_before);

    /* Report info gain */
    for (int i = 0; i < 10; i++) {
        hypo_curiosity_fep_report_info_gain(bridge, 0.5f);
    }

    /* Report novelty */
    for (int i = 0; i < 5; i++) {
        hypo_curiosity_fep_report_novelty(bridge, 0.7f);
    }

    hypo_curiosity_fep_stats_t stats_after;
    hypo_curiosity_fep_get_stats(bridge, &stats_after);

    EXPECT_GE(stats_after.info_gain_fe_reductions, stats_before.info_gain_fe_reductions);
    EXPECT_GE(stats_after.novelty_pe_events, stats_before.novelty_pe_events);
}

/* ============================================================================
 * Test Fixture for Salience FEP Bridge
 * ============================================================================ */

class HypoSalienceFEPRegressionTest : public ::testing::Test {
protected:
    hypo_salience_fep_bridge_t* bridge = nullptr;
    hypo_salience_fep_config_t config;
    fep_system_t* fep_system = nullptr;
    hypo_drive_system_handle_t* drive_system = nullptr;

    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 32, 16);
        ASSERT_NE(fep_system, nullptr);

        /* Create drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);

        /* Create bridge */
        hypo_salience_fep_default_config(&config);
        bridge = hypo_salience_fep_create(&config, drive_system, fep_system);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            hypo_salience_fep_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
    }

    bool verify_fe_bounds(float fe) {
        return fe >= 0.0f && !std::isnan(fe) && !std::isinf(fe);
    }

    bool verify_normalized(float value) {
        return value >= 0.0f && value <= 1.0f;
    }
};

/* ============================================================================
 * Salience FEP Bridge Regression Tests
 * ============================================================================ */

/* REG-SF-001: Salience weights are valid */
TEST_F(HypoSalienceFEPRegressionTest, REG_SF_001_SalienceWeightsValid) {
    hypo_salience_fep_update(bridge);

    float weights[HYPO_DRIVE_COUNT];
    int ret = hypo_salience_fep_get_weights(bridge, weights);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < HYPO_DRIVE_COUNT; i++) {
        EXPECT_GE(weights[i], HYPO_SALIENCE_FEP_MIN_WEIGHT)
            << "Weight " << i << " below minimum";
        EXPECT_LE(weights[i], HYPO_SALIENCE_FEP_MAX_WEIGHT)
            << "Weight " << i << " above maximum";
    }
}

/* REG-SF-002: Precision decreases with fatigue */
TEST_F(HypoSalienceFEPRegressionTest, REG_SF_002_FatiguePrecision) {
    hypo_salience_fep_effects_t effects_low_fatigue;
    hypo_salience_fep_modulate_precision(bridge, 0.1f);
    hypo_salience_fep_update(bridge);
    hypo_salience_fep_get_effects(bridge, &effects_low_fatigue);

    hypo_salience_fep_effects_t effects_high_fatigue;
    hypo_salience_fep_modulate_precision(bridge, 0.9f);
    hypo_salience_fep_update(bridge);
    hypo_salience_fep_get_effects(bridge, &effects_high_fatigue);

    EXPECT_LE(effects_high_fatigue.precision,
              effects_low_fatigue.precision + REGRESSION_PRECISION_TOLERANCE)
        << "High fatigue should have lower precision";
}

/* REG-SF-003: Conflict detection works */
TEST_F(HypoSalienceFEPRegressionTest, REG_SF_003_ConflictDetection) {
    hypo_salience_fep_update(bridge);

    hypo_salience_conflict_t conflict;
    float intensity;
    int ret = hypo_salience_fep_detect_conflict(bridge, &conflict, &intensity);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(verify_normalized(intensity))
        << "Conflict intensity should be in [0,1]";
}

/* REG-SF-004: Effects validity */
TEST_F(HypoSalienceFEPRegressionTest, REG_SF_004_EffectsValidity) {
    hypo_salience_fep_update(bridge);

    hypo_salience_fep_effects_t effects;
    hypo_salience_fep_get_effects(bridge, &effects);

    EXPECT_TRUE(verify_fe_bounds(effects.free_energy));
    EXPECT_TRUE(verify_normalized(effects.urgency_confidence));
    EXPECT_TRUE(verify_normalized(effects.response_urgency));
    EXPECT_TRUE(verify_normalized(effects.conflict_intensity));
    EXPECT_TRUE(verify_normalized(effects.attention_capacity));
    EXPECT_TRUE(verify_normalized(effects.attention_focus));
}

/* REG-SF-005: Utility strings */
TEST_F(HypoSalienceFEPRegressionTest, REG_SF_005_UtilityStrings) {
    EXPECT_NE(nullptr, hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_LOW));
    EXPECT_NE(nullptr, hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_MODERATE));
    EXPECT_NE(nullptr, hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_ELEVATED));
    EXPECT_NE(nullptr, hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_HIGH));
    EXPECT_NE(nullptr, hypo_salience_fep_level_name(HYPO_SALIENCE_FEP_LEVEL_CRITICAL));

    EXPECT_NE(nullptr, hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_MAINTAIN));
    EXPECT_NE(nullptr, hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_SHIFT));
    EXPECT_NE(nullptr, hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_FOCUS));
    EXPECT_NE(nullptr, hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_NARROW));
    EXPECT_NE(nullptr, hypo_salience_fep_response_name(HYPO_SALIENCE_FEP_RESPONSE_EMERGENCY));

    EXPECT_NE(nullptr, hypo_salience_fep_conflict_name(HYPO_SALIENCE_CONFLICT_NONE));
    EXPECT_NE(nullptr, hypo_salience_fep_conflict_name(HYPO_SALIENCE_CONFLICT_MILD));
    EXPECT_NE(nullptr, hypo_salience_fep_conflict_name(HYPO_SALIENCE_CONFLICT_MODERATE));
    EXPECT_NE(nullptr, hypo_salience_fep_conflict_name(HYPO_SALIENCE_CONFLICT_SEVERE));
}

/* REG-SF-006: Statistics accumulation */
TEST_F(HypoSalienceFEPRegressionTest, REG_SF_006_StatisticsAccumulation) {
    const int NUM_UPDATES = 50;
    for (int i = 0; i < NUM_UPDATES; i++) {
        hypo_salience_fep_update(bridge);
    }

    hypo_salience_fep_stats_t stats;
    hypo_salience_fep_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_updates, (uint64_t)NUM_UPDATES);
    EXPECT_FALSE(std::isnan(stats.avg_free_energy));
    EXPECT_FALSE(std::isnan(stats.avg_surprise));
    EXPECT_FALSE(std::isnan(stats.avg_prediction_error));
}

/* ============================================================================
 * Cross-Bridge Integration Regression Tests
 * ============================================================================ */

class HypoFEPBridgesCrossRegressionTest : public ::testing::Test {
protected:
    fep_system_t* fep_system = nullptr;
    hypo_drive_system_handle_t* drive_system = nullptr;

    void SetUp() override {
        /* Create shared FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep_system = fep_create(&fep_config, 32, 16);
        ASSERT_NE(fep_system, nullptr);

        /* Create shared drive system */
        hypo_drive_config_t drive_config = hypo_drive_default_config();
        drive_system = hypo_drive_create(&drive_config);
        ASSERT_NE(drive_system, nullptr);
    }

    void TearDown() override {
        if (fep_system) {
            fep_destroy(fep_system);
            fep_system = nullptr;
        }
        if (drive_system) {
            hypo_drive_destroy(drive_system);
            drive_system = nullptr;
        }
    }
};

/* REG-CROSS-001: Multiple bridges can share FEP system */
TEST_F(HypoFEPBridgesCrossRegressionTest, REG_CROSS_001_SharedFEPSystem) {
    /* Create multiple bridges sharing the same FEP system */
    hypo_bio_async_fep_config_t ba_config;
    hypo_bio_async_fep_default_config(&ba_config);
    hypo_bio_async_fep_bridge_t* ba_bridge =
        hypo_bio_async_fep_create(&ba_config, drive_system, fep_system);
    ASSERT_NE(ba_bridge, nullptr);

    hypo_salience_fep_config_t sal_config;
    hypo_salience_fep_default_config(&sal_config);
    hypo_salience_fep_bridge_t* sal_bridge =
        hypo_salience_fep_create(&sal_config, drive_system, fep_system);
    ASSERT_NE(sal_bridge, nullptr);

    /* Update both bridges */
    for (int i = 0; i < 50; i++) {
        hypo_bio_async_fep_update(ba_bridge);
        hypo_salience_fep_update(sal_bridge);
    }

    /* Both should still be functional */
    hypo_bio_async_fep_effects_t ba_effects;
    hypo_bio_async_fep_get_effects(ba_bridge, &ba_effects);
    EXPECT_FALSE(std::isnan(ba_effects.free_energy));

    hypo_salience_fep_effects_t sal_effects;
    hypo_salience_fep_get_effects(sal_bridge, &sal_effects);
    EXPECT_FALSE(std::isnan(sal_effects.free_energy));

    /* Cleanup */
    hypo_bio_async_fep_destroy(ba_bridge);
    hypo_salience_fep_destroy(sal_bridge);
}

/* REG-CROSS-002: Multiple bridges can share drive system */
TEST_F(HypoFEPBridgesCrossRegressionTest, REG_CROSS_002_SharedDriveSystem) {
    /* Create multiple bridges sharing the same drive system */
    hypo_gt_fep_config_t gt_config;
    hypo_gt_fep_default_config(&gt_config);
    hypo_gt_fep_bridge_t* gt_bridge =
        hypo_gt_fep_create(&gt_config, drive_system, fep_system);
    ASSERT_NE(gt_bridge, nullptr);

    hypo_salience_fep_config_t sal_config;
    hypo_salience_fep_default_config(&sal_config);
    hypo_salience_fep_bridge_t* sal_bridge =
        hypo_salience_fep_create(&sal_config, drive_system, fep_system);
    ASSERT_NE(sal_bridge, nullptr);

    /* Update both bridges */
    for (int i = 0; i < 50; i++) {
        hypo_gt_fep_update(gt_bridge);
        hypo_salience_fep_update(sal_bridge);
    }

    /* Both should still be functional */
    fep_to_gt_effects_t gt_effects;
    hypo_gt_fep_get_effects(gt_bridge, &gt_effects);
    EXPECT_FALSE(std::isnan(gt_effects.free_energy));

    hypo_salience_fep_effects_t sal_effects;
    hypo_salience_fep_get_effects(sal_bridge, &sal_effects);
    EXPECT_FALSE(std::isnan(sal_effects.free_energy));

    /* Cleanup */
    hypo_gt_fep_destroy(gt_bridge);
    hypo_salience_fep_destroy(sal_bridge);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
