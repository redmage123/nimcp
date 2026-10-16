/**
 * @file test_jepa_fep_bridge_integration.cpp
 * @brief Integration tests for JEPA FEP Bridge + FEP Orchestrator
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests real integration between JEPA FEP bridge and FEP orchestrator
 * WHY:  Verify embedding predictions minimize free energy, representation
 *       quality affects free energy, and collapse detection integrates properly
 * HOW:  Use real module instances (no mocks) to test actual interactions
 *
 * Tests cover:
 * - Embedding predictions with FEP integration
 * - Representation quality effects on free energy
 * - Latent space prediction error
 * - Joint embedding alignment
 * - Predictive encoding integration
 * - Abstraction level and free energy
 * - Temporal predictions in latent space
 * - Cross-modal embedding integration
 * - FEP update cycle verification (25ms intervals)
 * - Statistics accumulation across cycles
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/jepa/nimcp_jepa_fep_bridge.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

static const float TEST_EPSILON = 1e-4f;
static const uint64_t FEP_UPDATE_INTERVAL_MS = 50;  /* Default orchestrator interval */
static const uint64_t JEPA_UPDATE_INTERVAL_MS = 25; /* JEPA-specific interval */

/* ============================================================================
 * Test Fixture: JEPA FEP Bridge Integration
 * ============================================================================ */

class JepaFepBridgeIntegrationTest : public ::testing::Test {
protected:
    jepa_fep_bridge_t* bridge = nullptr;
    jepa_fep_config_t bridge_config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;

    void SetUp() override {
        /* Create JEPA FEP bridge with default config */
        bridge_config = jepa_fep_config_default();
        bridge_config.enable_logging = false;
        bridge_config.enable_collapse_detection = true;
        bridge = jepa_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr);

        /* Start FEP orchestrator */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);
    }

    void TearDown() override {
        if (bridge) {
            /* Unregister before destroying if registered */
            if (jepa_fep_bridge_is_registered(bridge)) {
                jepa_fep_bridge_unregister(bridge);
            }
            jepa_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    /* Helper to register bridge with FEP and verify */
    void register_bridge_with_fep() {
        uint32_t bridge_id = 0;
        int ret = jepa_fep_bridge_register(bridge, fep_orch, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);
        ASSERT_TRUE(jepa_fep_bridge_is_registered(bridge));
        ASSERT_GT(bridge_id, 0u);
    }

    /* Helper to simulate embedding predictions with varying error */
    void simulate_embedding_predictions(int count, float base_error, float variance) {
        for (int i = 0; i < count; i++) {
            float error = base_error + (i % 10) * variance * 0.1f;
            EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, error), 0);
        }
    }

    /* Helper to simulate representation quality measurements */
    void simulate_representation_quality(int count, float base_quality, float variance) {
        for (int i = 0; i < count; i++) {
            float quality = base_quality + (i % 5) * variance * 0.05f;
            quality = std::min(1.0f, std::max(0.0f, quality));
            EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, quality), 0);
        }
    }
};

/* ============================================================================
 * Test 1: EmbeddingPredictionWithFEP
 * Verify embedding predictions minimize free energy through FEP coordination
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, EmbeddingPredictionWithFEP) {
    register_bridge_with_fep();

    /* Record initial free energy (should be baseline) */
    float initial_fe = jepa_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_NEAR(initial_fe, JEPA_FEP_BASELINE_FREE_ENERGY, TEST_EPSILON);

    /* Simulate good embedding predictions (low error) */
    simulate_embedding_predictions(10, 0.1f, 0.05f);

    /* Run FEP update cycles */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, start_time + (i * FEP_UPDATE_INTERVAL_MS));
    }

    /* Get free energy after good predictions */
    float fe_good = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Reset and simulate poor predictions (high error) */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
    simulate_embedding_predictions(10, 1.5f, 0.2f);

    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, start_time + 1000 + (i * FEP_UPDATE_INTERVAL_MS));
    }

    float fe_poor = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Good predictions should result in lower free energy */
    EXPECT_LT(fe_good, fe_poor);

    /* Verify statistics accumulated */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.embedding_predictions, 0u);
}

/* ============================================================================
 * Test 2: RepresentationQualityFreeEnergy
 * Verify good representations reduce free energy
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, RepresentationQualityFreeEnergy) {
    register_bridge_with_fep();

    /*
     * Test that free energy responds to representation quality.
     * The exact relationship depends on configuration weights.
     * We verify that:
     * 1. Low quality + high error produces higher FE than baseline
     * 2. High quality + low error produces FE closer to baseline
     */

    /* Scenario A: Poor conditions - high error, low quality */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 1.5f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.2f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_poor = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Scenario B: Good conditions - low error, high quality (using new bridge) */
    jepa_fep_bridge_t* good_bridge = jepa_fep_bridge_create(&bridge_config);
    ASSERT_NE(good_bridge, nullptr);

    uint32_t bridge_id2 = 0;
    ASSERT_EQ(jepa_fep_bridge_register(good_bridge, fep_orch, nullptr, &bridge_id2), 0);

    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(good_bridge, 0.1f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(good_bridge, 0.95f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(good_bridge), 0);
    float fe_good = jepa_fep_bridge_get_free_energy_contribution(good_bridge);

    /* Poor conditions should produce higher free energy than good conditions */
    EXPECT_GT(fe_poor, fe_good)
        << "Poor condition FE (" << fe_poor << ") should exceed good condition FE ("
        << fe_good << ")";

    /* Both should be non-negative */
    EXPECT_GE(fe_poor, 0.0f);
    EXPECT_GE(fe_good, 0.0f);

    /* Both should be within max bounds */
    EXPECT_LE(fe_poor, JEPA_FEP_MAX_FREE_ENERGY);
    EXPECT_LE(fe_good, JEPA_FEP_MAX_FREE_ENERGY);

    /* Clean up */
    jepa_fep_bridge_unregister(good_bridge);
    jepa_fep_bridge_destroy(good_bridge);

    /* Verify bridge state is valid after stress */
    jepa_fep_state_t state = jepa_fep_bridge_get_state(bridge);
    EXPECT_TRUE(state == JEPA_FEP_STATE_ACTIVE || state == JEPA_FEP_STATE_DEGRADED)
        << "Expected ACTIVE or DEGRADED, got " << jepa_fep_state_name(state);
}

/* ============================================================================
 * Test 3: LatentSpacePredictionError
 * Verify latent predictions affect free energy correctly
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, LatentSpacePredictionError) {
    register_bridge_with_fep();

    /* Simulate varying prediction errors to model latent space predictions */
    std::vector<float> prediction_errors = {0.1f, 0.3f, 0.5f, 0.2f, 0.1f};
    std::vector<float> free_energies;

    uint64_t base_time = get_current_time_ms();

    for (size_t i = 0; i < prediction_errors.size(); i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, prediction_errors[i]), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
        free_energies.push_back(jepa_fep_bridge_get_free_energy_contribution(bridge));

        /* Run FEP cycle */
        fep_orchestrator_update(fep_orch, base_time + (i * FEP_UPDATE_INTERVAL_MS));
    }

    /* Verify free energy tracks prediction error */
    /* Higher error (index 2) should have higher FE than lower error (index 0) */
    EXPECT_GT(free_energies[2], free_energies[0]);

    /* Final low error should reduce FE */
    EXPECT_LT(free_energies[4], free_energies[2]);
}

/* ============================================================================
 * Test 4: JointEmbeddingAlignment
 * Verify joint embedding alignment reduces uncertainty/free energy
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, JointEmbeddingAlignment) {
    register_bridge_with_fep();

    /* Simulate misaligned embeddings (high error, low quality) */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 1.0f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.3f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_misaligned = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Reset and simulate aligned embeddings */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.1f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.95f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_aligned = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Aligned embeddings should have much lower free energy */
    EXPECT_LT(fe_aligned, fe_misaligned);

    /* The difference should be significant */
    float reduction = (fe_misaligned - fe_aligned) / fe_misaligned;
    EXPECT_GT(reduction, 0.3f);  /* At least 30% reduction */
}

/* ============================================================================
 * Test 5: PredictiveEncodingIntegration
 * Verify predictive encoding reduces prediction error over time
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, PredictiveEncodingIntegration) {
    register_bridge_with_fep();

    /* Simulate learning curve: prediction error decreases over time */
    uint64_t base_time = get_current_time_ms();
    float prev_fe = JEPA_FEP_MAX_FREE_ENERGY;

    for (int epoch = 0; epoch < 10; epoch++) {
        /* Decreasing prediction error simulates learning */
        float error = 1.5f * expf(-0.3f * epoch);  /* Exponential decay */
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, error), 0);

        /* Increasing quality simulates improving representations */
        float quality = 0.5f + 0.05f * epoch;
        quality = std::min(quality, 1.0f);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, quality), 0);

        /* Run FEP update */
        fep_orchestrator_update(fep_orch, base_time + (epoch * FEP_UPDATE_INTERVAL_MS));
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

        float current_fe = jepa_fep_bridge_get_free_energy_contribution(bridge);

        /* Free energy should generally decrease (with some tolerance for noise) */
        if (epoch > 2) {
            EXPECT_LE(current_fe, prev_fe + 0.1f);
        }
        prev_fe = current_fe;
    }

    /* Final free energy should be significantly lower than start */
    float final_fe = jepa_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_LT(final_fe, 1.0f);
}

/* ============================================================================
 * Test 6: AbstractionLevelFreeEnergy
 * Verify abstraction quality affects free energy
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, AbstractionLevelFreeEnergy) {
    register_bridge_with_fep();

    /* Test different abstraction levels (simulated via quality) */
    struct AbstractionLevel {
        float quality;
        float expected_max_fe;
    };

    std::vector<AbstractionLevel> levels = {
        {0.1f, 1.5f},  /* Poor abstraction */
        {0.5f, 1.0f},  /* Medium abstraction */
        {0.9f, 0.5f},  /* Good abstraction */
    };

    for (const auto& level : levels) {
        EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, level.quality), 0);
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

        float fe = jepa_fep_bridge_get_free_energy_contribution(bridge);
        EXPECT_LE(fe, level.expected_max_fe)
            << "Quality " << level.quality << " should have FE <= " << level.expected_max_fe;
    }
}

/* ============================================================================
 * Test 7: TemporalPredictionJEPA
 * Verify temporal predictions in latent space
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, TemporalPredictionJEPA) {
    register_bridge_with_fep();

    /* Simulate temporal sequence of predictions */
    uint64_t base_time = get_current_time_ms();
    std::vector<float> temporal_errors;

    /* Time series with varying prediction accuracy */
    for (int t = 0; t < 20; t++) {
        /* Sinusoidal error pattern simulating temporal prediction difficulty */
        float error = 0.3f + 0.2f * sinf(t * 0.5f);
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, error), 0);

        /* Run FEP update at regular intervals */
        if (t % 2 == 0) {
            fep_orchestrator_update(fep_orch, base_time + (t * JEPA_UPDATE_INTERVAL_MS));
        }

        temporal_errors.push_back(error);
    }

    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

    /* Verify statistics captured temporal data */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.embedding_predictions, 20u);

    /* Average error should be close to 0.3 */
    EXPECT_NEAR(stats.avg_embedding_error, 0.3f, 0.1f);
}

/* ============================================================================
 * Test 8: CrossModalEmbedding
 * Verify cross-modal embeddings and prediction error
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, CrossModalEmbedding) {
    register_bridge_with_fep();

    /* Simulate cross-modal integration (e.g., visual + audio embeddings) */

    /* Phase 1: Single modality (good within-modality predictions) */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.2f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.85f), 0);
    }
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_single_modal = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Phase 2: Cross-modal (initially higher error as modalities integrate) */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.6f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.7f), 0);
    }
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_cross_modal_initial = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Phase 3: Cross-modal after learning (improved predictions) */
    EXPECT_EQ(jepa_fep_bridge_reset(bridge), 0);
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.15f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.92f), 0);
    }
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_cross_modal_learned = jepa_fep_bridge_get_free_energy_contribution(bridge);

    /* Cross-modal learning should improve FE over initial cross-modal */
    EXPECT_LT(fe_cross_modal_learned, fe_cross_modal_initial);

    /* Learned cross-modal should approach single-modal performance */
    EXPECT_LT(fe_cross_modal_learned, fe_single_modal + 0.1f);
}

/* ============================================================================
 * Test 9: FEPUpdateCycleIntegration
 * Verify FEP update cycles work correctly at expected intervals
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, FEPUpdateCycleIntegration) {
    register_bridge_with_fep();

    /* Configure shorter update interval for testing */
    fep_orchestrator_set_update_interval(fep_orch, FEP_BRIDGE_CATEGORY_JEPA,
                                          JEPA_UPDATE_INTERVAL_MS);

    /* Reset bridge stats */
    EXPECT_EQ(jepa_fep_bridge_reset_stats(bridge), 0);

    /* Seed with some data */
    EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.3f), 0);
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.8f), 0);

    /* Run FEP updates at the expected interval */
    uint64_t base_time = get_current_time_ms();
    int expected_updates = 0;

    for (int i = 0; i < 20; i++) {
        uint64_t current_time = base_time + (i * JEPA_UPDATE_INTERVAL_MS);
        int updated = fep_orchestrator_update(fep_orch, current_time);

        if (updated >= 0) {
            expected_updates += updated;
        }
    }

    /* Verify bridge was updated multiple times */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    EXPECT_GT(stats.total_updates, 0u);

    /* Verify FEP orchestrator stats */
    fep_orchestrator_stats_t orch_stats;
    EXPECT_EQ(fep_orchestrator_get_stats(fep_orch, &orch_stats), 0);
    EXPECT_GT(orch_stats.total_update_cycles, 0u);
}

/* ============================================================================
 * Test 10: StatisticsAccumulation
 * Verify stats accumulate correctly across multiple cycles
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, StatisticsAccumulation) {
    register_bridge_with_fep();

    /* Reset all stats */
    EXPECT_EQ(jepa_fep_bridge_reset_stats(bridge), 0);
    fep_orchestrator_reset_stats(fep_orch);

    /* Record known number of samples */
    const int NUM_PREDICTIONS = 15;
    const int NUM_QUALITY_UPDATES = 10;
    float total_error = 0.0f;
    float min_quality = 1.0f;

    for (int i = 0; i < NUM_PREDICTIONS; i++) {
        float error = 0.1f + (i * 0.05f);
        total_error += error;
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, error), 0);
    }

    for (int i = 0; i < NUM_QUALITY_UPDATES; i++) {
        float quality = 0.9f - (i * 0.08f);
        quality = std::max(0.1f, quality);
        if (quality < min_quality) min_quality = quality;
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, quality), 0);
    }

    /* Run several FEP updates */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
        fep_orchestrator_update(fep_orch, base_time + (i * FEP_UPDATE_INTERVAL_MS));
    }

    /* Verify statistics */
    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);

    EXPECT_EQ(stats.embedding_predictions, (uint64_t)NUM_PREDICTIONS);
    EXPECT_EQ(stats.representation_updates, (uint64_t)NUM_QUALITY_UPDATES);
    /* At least 10 updates from force_update calls (may have more from FEP orchestrator) */
    EXPECT_GE(stats.total_updates, 10u);
    EXPECT_GT(stats.total_free_energy_contribution, 0.0f);
    EXPECT_GT(stats.peak_free_energy, 0.0f);
    EXPECT_NEAR(stats.min_representation_quality, min_quality, 0.01f);

    /* Average error should be calculated correctly */
    float expected_avg = total_error / NUM_PREDICTIONS;
    EXPECT_NEAR(stats.avg_embedding_error, expected_avg, 0.05f);
}

/* ============================================================================
 * Test 11: CollapseDetectionIntegration
 * Verify representation collapse detection integrates with FEP
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, CollapseDetectionIntegration) {
    /* Configure collapse detection */
    bridge_config.enable_collapse_detection = true;
    bridge_config.collapse_detection_threshold = 0.3f;
    bridge_config.representation_collapse_penalty = 0.5f;
    EXPECT_EQ(jepa_fep_bridge_set_config(bridge, &bridge_config), 0);

    register_bridge_with_fep();

    /* Record quality above threshold - no collapse */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.5f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_no_collapse = jepa_fep_bridge_get_free_energy_contribution(bridge);

    jepa_fep_stats_t stats;
    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    uint64_t collapse_count_before = stats.collapse_detections;

    /* Record quality below threshold - collapse */
    EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.1f), 0);
    EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);
    float fe_collapse = jepa_fep_bridge_get_free_energy_contribution(bridge);

    EXPECT_EQ(jepa_fep_bridge_get_stats(bridge, &stats), 0);
    uint64_t collapse_count_after = stats.collapse_detections;

    /* Collapse should increase free energy and be detected */
    EXPECT_GT(fe_collapse, fe_no_collapse);
    EXPECT_GT(collapse_count_after, collapse_count_before);

    /* Run FEP cycles to verify collapse persists */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 5; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * FEP_UPDATE_INTERVAL_MS));
    }

    /* State might be degraded due to collapse */
    jepa_fep_state_t state = jepa_fep_bridge_get_state(bridge);
    /* State depends on overall free energy vs threshold */
    EXPECT_TRUE(state == JEPA_FEP_STATE_ACTIVE || state == JEPA_FEP_STATE_DEGRADED);
}

/* ============================================================================
 * Test 12: MultipleUpdateCyclesConvergence
 * Verify that free energy converges with consistent good predictions
 * ============================================================================ */

TEST_F(JepaFepBridgeIntegrationTest, MultipleUpdateCyclesConvergence) {
    register_bridge_with_fep();

    /* Provide consistent good predictions */
    uint64_t base_time = get_current_time_ms();
    std::vector<float> fe_history;

    for (int cycle = 0; cycle < 30; cycle++) {
        /* Consistent low error and high quality */
        EXPECT_EQ(jepa_fep_bridge_record_prediction_error(bridge, 0.1f), 0);
        EXPECT_EQ(jepa_fep_bridge_record_representation_quality(bridge, 0.9f), 0);

        fep_orchestrator_update(fep_orch, base_time + (cycle * FEP_UPDATE_INTERVAL_MS));
        EXPECT_EQ(jepa_fep_bridge_force_update(bridge), 0);

        fe_history.push_back(jepa_fep_bridge_get_free_energy_contribution(bridge));
    }

    /* Free energy should stabilize (low variance in later cycles) */
    float sum_late = 0.0f;
    float sum_sq_late = 0.0f;
    int late_start = 20;
    int late_count = fe_history.size() - late_start;

    for (size_t i = late_start; i < fe_history.size(); i++) {
        sum_late += fe_history[i];
        sum_sq_late += fe_history[i] * fe_history[i];
    }

    float mean_late = sum_late / late_count;
    float var_late = (sum_sq_late / late_count) - (mean_late * mean_late);

    /* Variance should be low in stable state */
    EXPECT_LT(var_late, 0.01f);

    /* Final free energy should be close to baseline (good predictions) */
    EXPECT_LT(fe_history.back(), 0.5f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
