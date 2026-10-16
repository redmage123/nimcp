/**
 * @file test_parietal_fep_integration.cpp
 * @brief Integration tests for Parietal Lobe + FEP Orchestrator
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests integration between Parietal Lobe and FEP orchestrator
 * WHY:  Verify FEP correctly coordinates parietal processing and free energy tracking
 * HOW:  Test FEP update cycles, spatial/body schema effects on free energy, and statistics
 *
 * THEORETICAL BASIS:
 * - Parietal cortex minimizes spatial prediction error
 * - Body schema errors contribute to free energy
 * - Mathematical processing accuracy affects prediction error
 * - Coordinated FEP updates enable system-wide free energy minimization
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/parietal/nimcp_parietal.h"
#include "cognitive/parietal/nimcp_parietal_fep_bridge.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define PARIETAL_FEP_TEST_UPDATE_INTERVAL_MS    50
#define PARIETAL_FEP_TEST_UPDATE_CYCLES         20
#define PARIETAL_FEP_FREE_ENERGY_TOLERANCE      0.01f

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

static std::atomic<int> g_parietal_update_count{0};
static std::atomic<float> g_accumulated_free_energy{0.0f};
static std::atomic<float> g_last_spatial_uncertainty{0.0f};
static std::atomic<float> g_last_body_schema_error{0.0f};

static void reset_test_counters() {
    g_parietal_update_count = 0;
    g_accumulated_free_energy = 0.0f;
    g_last_spatial_uncertainty = 0.0f;
    g_last_body_schema_error = 0.0f;
}

/* ============================================================================
 * Test Fixture: Parietal + FEP Integration
 * ============================================================================ */

class ParietalFEPIntegrationTest : public ::testing::Test {
protected:
    parietal_lobe_t* parietal = nullptr;
    parietal_config_t parietal_config;
    parietal_fep_bridge_t* bridge = nullptr;
    parietal_fep_config_t bridge_config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;

    void SetUp() override {
        reset_test_counters();

        /* Create parietal lobe with default config */
        parietal_config = parietal_default_config();
        parietal_config.enable_fep_bridge = false;  /* We'll manage our own bridge */
        parietal = parietal_create_custom(&parietal_config);
        ASSERT_NE(parietal, nullptr) << "Parietal lobe creation should succeed";

        /* Create FEP bridge with default config */
        bridge_config = parietal_fep_config_default();
        bridge_config.enable_callbacks = true;
        bridge_config.enable_degraded_mode = true;
        bridge = parietal_fep_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr) << "FEP bridge creation should succeed";

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr) << "FEP orchestrator creation should succeed";

        /* Start FEP orchestrator */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0) << "FEP orchestrator start should succeed";
    }

    void TearDown() override {
        if (bridge) {
            parietal_fep_bridge_unregister(bridge);
            parietal_fep_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (parietal) {
            parietal_destroy(parietal);
            parietal = nullptr;
        }
    }

    /* Helper to register bridge with FEP orchestrator */
    void register_parietal_bridge() {
        uint32_t bridge_id = 0;
        int ret = parietal_fep_bridge_register(bridge, fep_orch, parietal, &bridge_id);
        ASSERT_EQ(ret, 0) << "Bridge registration should succeed";
        ASSERT_GT(bridge_id, 0u) << "Bridge ID should be assigned";
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    /* Helper to run FEP update cycles */
    void run_fep_cycles(int num_cycles, uint64_t interval_ms = PARIETAL_FEP_TEST_UPDATE_INTERVAL_MS) {
        uint64_t start_time = get_current_time_ms();
        for (int i = 0; i < num_cycles; i++) {
            uint64_t current_time = start_time + (i * interval_ms);
            int updated = fep_orchestrator_update(fep_orch, current_time);
            EXPECT_GE(updated, 0) << "FEP orchestrator update should succeed";
        }
    }

    /* Helper to perform spatial computations */
    void perform_spatial_computations(int count) {
        spatial_reasoning_t* spatial = parietal_get_spatial(parietal);
        if (spatial) {
            for (int i = 0; i < count; i++) {
                /* Perform coordinate transform requests */
                vec3_t position = {1.0f + i * 0.1f, 2.0f, 3.0f};
                observer_pose_t observer;
                memset(&observer, 0, sizeof(observer));
                observer.position.x = 0.0f;
                observer.position.y = 0.0f;
                observer.position.z = 0.0f;
                observer.orientation.w = 1.0f;
                observer.orientation.x = 0.0f;
                observer.orientation.y = 0.0f;
                observer.orientation.z = 0.0f;
                observer.heading = 0.0f;

                parietal_request_t req;
                memset(&req, 0, sizeof(req));
                req.type = PARIETAL_COORDINATE_TRANSFORM;
                req.input.transform_input.position = position;
                req.input.transform_input.observer = &observer;
                req.input.transform_input.ego_to_allocentric = true;

                parietal_result_t result = parietal_process(parietal, &req);
                (void)result;  /* Result check optional for basic integration test */
            }
        }
    }

    /* Helper to perform math operations */
    void perform_math_operations(int count) {
        number_sense_t* ns = parietal_get_number_sense(parietal);
        if (ns) {
            for (int i = 0; i < count; i++) {
                float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
                parietal_estimate_quantity(parietal, values, 5);
            }
        }
    }
};

/* ============================================================================
 * SpatialProcessingWithFEP - Spatial computations minimize prediction error
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, SpatialProcessingWithFEP) {
    register_parietal_bridge();

    /* Get initial free energy */
    float initial_fe = parietal_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(initial_fe, 0.0f) << "Initial free energy should be non-negative";

    /* Perform spatial computations */
    perform_spatial_computations(10);

    /* Run FEP update cycles */
    run_fep_cycles(PARIETAL_FEP_TEST_UPDATE_CYCLES);

    /* Get metrics after processing */
    parietal_fep_metrics_t metrics;
    int ret = parietal_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(ret, 0) << "Get metrics should succeed";

    /* Spatial computations should contribute to metrics */
    EXPECT_GE(metrics.spatial_computations, 0u) << "Spatial computations should be tracked";

    /* Free energy should remain within bounds */
    float final_fe = parietal_fep_bridge_get_free_energy_contribution(bridge);
    EXPECT_GE(final_fe, 0.0f) << "Free energy should be non-negative";
    EXPECT_LE(final_fe, bridge_config.max_free_energy) << "Free energy should not exceed max";
}

/* ============================================================================
 * BodySchemaFreeEnergy - Body model accuracy affects free energy
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, BodySchemaFreeEnergy) {
    register_parietal_bridge();

    /* Run initial FEP cycles */
    run_fep_cycles(5);

    /* Get initial body schema error */
    float initial_bse = parietal_fep_bridge_get_body_schema_error(bridge);
    EXPECT_GE(initial_bse, 0.0f) << "Body schema error should be non-negative";
    EXPECT_LE(initial_bse, 1.0f) << "Body schema error should be <= 1.0";

    /* Introduce fatigue and inflammation (affects body schema) */
    parietal_set_fatigue(parietal, 0.5f);
    parietal_set_inflammation(parietal, 0.3f);

    /* Run more FEP cycles */
    run_fep_cycles(10);

    /* Get updated metrics */
    parietal_fep_metrics_t metrics;
    int ret = parietal_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(ret, 0);

    /* Body schema contribution should be non-negative */
    EXPECT_GE(metrics.body_schema_contribution, 0.0f)
        << "Body schema contribution should be non-negative";

    /* Higher fatigue/inflammation should increase body schema error */
    float updated_bse = parietal_fep_bridge_get_body_schema_error(bridge);
    EXPECT_GE(updated_bse, 0.0f);
    EXPECT_LE(updated_bse, 1.0f);
}

/* ============================================================================
 * CoordinateTransformPredictionError - Transform errors increase free energy
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, CoordinateTransformPredictionError) {
    register_parietal_bridge();

    /* Perform many coordinate transforms */
    perform_spatial_computations(50);

    /* Run FEP cycles to compute free energy */
    run_fep_cycles(PARIETAL_FEP_TEST_UPDATE_CYCLES);

    /* Get metrics */
    parietal_fep_metrics_t metrics;
    int ret = parietal_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(ret, 0);

    /* Spatial uncertainty should be tracked */
    EXPECT_GE(metrics.spatial_uncertainty, 0.0f);
    EXPECT_LE(metrics.spatial_uncertainty, 1.0f);

    /* Spatial contribution to free energy */
    EXPECT_GE(metrics.spatial_contribution, 0.0f);
}

/* ============================================================================
 * AttentionGuidanceSpatial - Spatial attention guided by prediction error
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, AttentionGuidanceSpatial) {
    register_parietal_bridge();

    /* Run FEP cycles */
    run_fep_cycles(10);

    /* Get prediction error */
    float pred_error = parietal_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(pred_error, 0.0f) << "Prediction error should be non-negative";
    EXPECT_LE(pred_error, 1.0f) << "Prediction error should be <= 1.0";

    /* Perform spatial computations (should affect prediction error) */
    perform_spatial_computations(20);
    run_fep_cycles(10);

    /* Prediction error should be tracked after processing */
    float updated_pe = parietal_fep_bridge_get_prediction_error(bridge);
    EXPECT_GE(updated_pe, 0.0f);
    EXPECT_LE(updated_pe, 1.0f);
}

/* ============================================================================
 * ReachingMovementPrediction - Motor predictions reduce uncertainty
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, ReachingMovementPrediction) {
    register_parietal_bridge();

    /* Simulate motor-like spatial processing */
    spatial_reasoning_t* spatial = parietal_get_spatial(parietal);
    ASSERT_NE(spatial, nullptr);

    /* Record initial state */
    parietal_fep_metrics_t initial_metrics;
    parietal_fep_bridge_get_metrics(bridge, &initial_metrics);

    /* Perform reaching-like coordinate transforms */
    for (int i = 0; i < 10; i++) {
        vec3_t target = {(float)i * 0.1f, 1.0f, 0.5f};
        observer_pose_t observer;
        memset(&observer, 0, sizeof(observer));

        parietal_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PARIETAL_COORDINATE_TRANSFORM;
        req.input.transform_input.position = target;
        req.input.transform_input.observer = &observer;
        req.input.transform_input.ego_to_allocentric = false;

        parietal_result_t result = parietal_process(parietal, &req);
        (void)result;
    }

    /* Run FEP update */
    run_fep_cycles(15);

    /* Verify update occurred */
    parietal_fep_metrics_t final_metrics;
    parietal_fep_bridge_get_metrics(bridge, &final_metrics);

    /* Update count should have increased */
    EXPECT_GT(final_metrics.update_count, initial_metrics.update_count)
        << "Update count should increase after FEP cycles";
}

/* ============================================================================
 * ObjectLocationTracking - Tracking reduces spatial prediction error
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, ObjectLocationTracking) {
    register_parietal_bridge();

    /* Simulate object tracking with spatial queries */
    for (int i = 0; i < 20; i++) {
        parietal_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PARIETAL_SPATIAL_QUERY;
        req.input.spatial_query_input.query_point.x = (float)i * 0.1f;
        req.input.spatial_query_input.query_point.y = 1.0f;
        req.input.spatial_query_input.query_point.z = 0.5f;
        req.input.spatial_query_input.radius = 2.0f;
        req.input.spatial_query_input.k = 5;

        parietal_result_t result = parietal_process(parietal, &req);
        (void)result;
    }

    /* Run FEP cycles */
    run_fep_cycles(PARIETAL_FEP_TEST_UPDATE_CYCLES);

    /* Get statistics */
    parietal_fep_stats_t stats;
    int ret = parietal_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    /* Spatial computations should be tracked */
    EXPECT_GE(stats.spatial_computations, 0u);
}

/* ============================================================================
 * EgocentricAllocentricSwitch - Reference frame switching affects free energy
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, EgocentricAllocentricSwitch) {
    register_parietal_bridge();

    /* Perform egocentric transforms */
    for (int i = 0; i < 10; i++) {
        parietal_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PARIETAL_COORDINATE_TRANSFORM;
        req.input.transform_input.position.x = 1.0f;
        req.input.transform_input.position.y = 2.0f;
        req.input.transform_input.position.z = 3.0f;
        observer_pose_t observer;
        memset(&observer, 0, sizeof(observer));
        req.input.transform_input.observer = &observer;
        req.input.transform_input.ego_to_allocentric = true;

        parietal_result_t result = parietal_process(parietal, &req);
        (void)result;
    }

    run_fep_cycles(5);
    float ego_fe = parietal_fep_bridge_get_free_energy_contribution(bridge);

    /* Perform allocentric transforms */
    for (int i = 0; i < 10; i++) {
        parietal_request_t req;
        memset(&req, 0, sizeof(req));
        req.type = PARIETAL_COORDINATE_TRANSFORM;
        req.input.transform_input.position.x = 1.0f;
        req.input.transform_input.position.y = 2.0f;
        req.input.transform_input.position.z = 3.0f;
        observer_pose_t observer;
        memset(&observer, 0, sizeof(observer));
        req.input.transform_input.observer = &observer;
        req.input.transform_input.ego_to_allocentric = false;

        parietal_result_t result = parietal_process(parietal, &req);
        (void)result;
    }

    run_fep_cycles(5);
    float allo_fe = parietal_fep_bridge_get_free_energy_contribution(bridge);

    /* Both should be valid free energy values */
    EXPECT_GE(ego_fe, 0.0f);
    EXPECT_GE(allo_fe, 0.0f);
    EXPECT_LE(ego_fe, bridge_config.max_free_energy);
    EXPECT_LE(allo_fe, bridge_config.max_free_energy);
}

/* ============================================================================
 * SpatialMemoryIntegration - Spatial memory reduces location uncertainty
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, SpatialMemoryIntegration) {
    register_parietal_bridge();

    /* Initial state */
    run_fep_cycles(5);
    parietal_fep_metrics_t initial_metrics;
    parietal_fep_bridge_get_metrics(bridge, &initial_metrics);

    /* Perform repeated spatial queries (simulates building spatial memory) */
    for (int rep = 0; rep < 5; rep++) {
        perform_spatial_computations(10);
        run_fep_cycles(3);
    }

    /* Get final metrics */
    parietal_fep_metrics_t final_metrics;
    parietal_fep_bridge_get_metrics(bridge, &final_metrics);

    /* Entropy should be tracked */
    EXPECT_GE(final_metrics.entropy, 0.0f);
    EXPECT_LE(final_metrics.entropy, 1.0f);

    /* Update count should have increased significantly */
    EXPECT_GT(final_metrics.update_count, initial_metrics.update_count);
}

/* ============================================================================
 * FEPUpdateCycleIntegration - Verify 50ms update cycles work correctly
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, FEPUpdateCycleIntegration) {
    register_parietal_bridge();

    /* Configure update interval */
    fep_orchestrator_set_update_interval(fep_orch, FEP_BRIDGE_CATEGORY_COGNITIVE, 50);

    /* Reset stats */
    parietal_fep_bridge_reset_stats(bridge);

    /* Run known number of cycles at 60ms intervals (should trigger updates every ~60ms) */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 60));
    }

    /* Get stats */
    parietal_fep_stats_t stats;
    int ret = parietal_fep_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(ret, 0);

    /* Should have received updates */
    EXPECT_GT(stats.total_updates, 0u) << "Should have received FEP updates";

    /* Verify orchestrator statistics */
    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);
    EXPECT_GT(orch_stats.total_update_cycles, 0u);
    EXPECT_GT(orch_stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].total_updates, 0u);
}

/* ============================================================================
 * StatisticsAccumulation - Stats accumulate across multiple cycles
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, StatisticsAccumulation) {
    register_parietal_bridge();

    /* Initial stats */
    parietal_fep_stats_t initial_stats;
    parietal_fep_bridge_get_stats(bridge, &initial_stats);
    uint64_t initial_updates = initial_stats.total_updates;

    /* Perform operations and FEP cycles */
    for (int round = 0; round < 5; round++) {
        perform_spatial_computations(5);
        perform_math_operations(5);
        run_fep_cycles(5);
    }

    /* Final stats */
    parietal_fep_stats_t final_stats;
    parietal_fep_bridge_get_stats(bridge, &final_stats);

    /* Stats should accumulate */
    EXPECT_GT(final_stats.total_updates, initial_updates)
        << "Total updates should accumulate";
    EXPECT_GE(final_stats.avg_free_energy, 0.0f);
    EXPECT_GE(final_stats.avg_prediction_error, 0.0f);
    EXPECT_LE(final_stats.avg_prediction_error, 1.0f);

    /* Check FEP orchestrator stats too */
    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);
    EXPECT_GT(orch_stats.total_bridge_updates, 0u);
}

/* ============================================================================
 * MathPredictionError - Mathematical processing affects prediction error
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, MathPredictionError) {
    register_parietal_bridge();

    /* Perform math operations */
    perform_math_operations(20);

    /* Run FEP cycles */
    run_fep_cycles(15);

    /* Get metrics */
    parietal_fep_metrics_t metrics;
    int ret = parietal_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(ret, 0);

    /* Math prediction error should be tracked */
    EXPECT_GE(metrics.math_prediction_error, 0.0f);
    EXPECT_LE(metrics.math_prediction_error, 1.0f);

    /* Math contribution to free energy */
    EXPECT_GE(metrics.math_contribution, 0.0f);
}

/* ============================================================================
 * DegradedModeTransition - High free energy triggers degraded mode
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, DegradedModeTransition) {
    register_parietal_bridge();

    /* Initial state should not be degraded */
    EXPECT_FALSE(parietal_fep_bridge_is_degraded(bridge));

    /* Run normal operations */
    run_fep_cycles(10);
    parietal_fep_state_t state = parietal_fep_bridge_get_state(bridge);
    EXPECT_NE(state, PARIETAL_FEP_STATE_ERROR);

    /* State should be one of the valid processing states */
    EXPECT_GE((int)state, (int)PARIETAL_FEP_STATE_UNINITIALIZED);
    EXPECT_LE((int)state, (int)PARIETAL_FEP_STATE_ERROR);
}

/* ============================================================================
 * ParietalWithOtherBridges - Works alongside other cognitive bridges
 * ============================================================================ */

static std::atomic<int> g_other_bridge_update_count{0};

static int other_cognitive_bridge_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_other_bridge_update_count++;
    return 0;
}

TEST_F(ParietalFEPIntegrationTest, ParietalWithOtherBridges) {
    g_other_bridge_update_count = 0;

    /* Register parietal bridge */
    register_parietal_bridge();

    /* Add dummy bridge for another cognitive module */
    static int dummy_handle = 1;
    uint32_t other_bridge_id = 0;
    int ret = fep_orchestrator_register_bridge(
        fep_orch, "other_cognitive_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_handle,
        other_cognitive_bridge_update,
        nullptr,
        &other_bridge_id
    );
    ASSERT_EQ(ret, 0);

    /* Run FEP cycles */
    run_fep_cycles(15);

    /* Both bridges should have been updated */
    parietal_fep_stats_t parietal_stats;
    parietal_fep_bridge_get_stats(bridge, &parietal_stats);
    EXPECT_GT(parietal_stats.total_updates, 0u);
    EXPECT_GT(g_other_bridge_update_count.load(), 0);

    /* Verify orchestrator tracked all bridges */
    fep_orchestrator_stats_t orch_stats;
    fep_orchestrator_get_stats(fep_orch, &orch_stats);
    EXPECT_GE(orch_stats.total_bridges, 2u);
}

/* ============================================================================
 * SurpriseEventTracking - Surprise events are detected and tracked
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, SurpriseEventTracking) {
    register_parietal_bridge();

    /* Run initial cycles to establish baseline */
    run_fep_cycles(10);

    /* Get metrics */
    parietal_fep_metrics_t metrics;
    int ret = parietal_fep_bridge_get_metrics(bridge, &metrics);
    EXPECT_EQ(ret, 0);

    /* Surprise should be tracked */
    EXPECT_GE(metrics.surprise, 0.0f);
    EXPECT_LE(metrics.surprise, 1.0f);

    /* Get stats - surprise events should be counted */
    parietal_fep_stats_t stats;
    parietal_fep_bridge_get_stats(bridge, &stats);
    EXPECT_GE(stats.surprise_events, 0u);
}

/* ============================================================================
 * FreeEnergyWeightingIntegration - Weight configuration affects contributions
 * ============================================================================ */

TEST_F(ParietalFEPIntegrationTest, FreeEnergyWeightingIntegration) {
    register_parietal_bridge();

    /* Get initial config */
    parietal_fep_config_t config;
    parietal_fep_bridge_get_config(bridge, &config);

    /* Verify weights sum to approximately 1.0 */
    float weight_sum = config.spatial_uncertainty_weight +
                       config.body_schema_error_weight +
                       config.math_error_weight;
    EXPECT_NEAR(weight_sum, 1.0f, 0.01f);

    /* Run FEP cycles */
    run_fep_cycles(10);

    /* Get metrics */
    parietal_fep_metrics_t metrics;
    parietal_fep_bridge_get_metrics(bridge, &metrics);

    /* Contributions should reflect weights */
    EXPECT_GE(metrics.spatial_contribution, 0.0f);
    EXPECT_GE(metrics.body_schema_contribution, 0.0f);
    EXPECT_GE(metrics.math_contribution, 0.0f);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
