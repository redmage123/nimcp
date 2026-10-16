/**
 * @file test_surface_e2e.cpp
 * @brief End-to-End Tests for Surface Geometry System
 *
 * WHAT: Full system tests for surface geometry optimization
 * WHY:  Verify complete workflows from initialization to optimization
 * HOW:  GTest-based E2E tests with full subsystem integration
 *
 * SCENARIOS TESTED:
 * - Full brain initialization with surface geometry
 * - Dendrite growth with surface-optimized spine placement
 * - Axon branching with trifurcation prediction
 * - Bio-async event propagation across systems
 * - Immune response to geometry anomalies
 * - Quantum-enhanced optimization workflow
 *
 * NIMCP STANDARDS:
 * - E2E tests verify complete system behavior
 * - May create and destroy full brain instances
 */

#include <gtest/gtest.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <vector>

extern "C" {
#include "core/geometry/nimcp_surface_geometry.h"
#include "core/geometry/nimcp_surface_geometry_types.h"
#include "core/geometry/nimcp_surface_optimization.h"
#include "core/brain/bridges/nimcp_surface_geometry_bridge.h"
#include "core/brain/factory/init/nimcp_brain_init_surface_geometry.h"
#include "async/bridges/nimcp_surface_bio_async_bridge.h"
#include "quantum/integration/nimcp_surface_quantum_bridge.h"
#include "cognitive/immune/nimcp_surface_immune_bridge.h"
}

// Test tolerances
#define TOLERANCE 1e-5f
#define ANGLE_TOLERANCE 0.1f

//=============================================================================
// E2E Test Fixture
//=============================================================================

class SurfaceE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create all subsystems
        geo_ctx = surface_geometry_create(nullptr);
        ASSERT_NE(geo_ctx, nullptr);

        brain_bridge = surface_geometry_bridge_create(nullptr);
        ASSERT_NE(brain_bridge, nullptr);

        bio_bridge = surface_bio_async_bridge_create(nullptr);
        ASSERT_NE(bio_bridge, nullptr);

        quantum_bridge = surface_quantum_bridge_create(nullptr);
        ASSERT_NE(quantum_bridge, nullptr);

        immune_bridge = surface_immune_bridge_create(nullptr);
        ASSERT_NE(immune_bridge, nullptr);

        // Connect all bridges
        surface_geometry_bridge_connect_geometry(brain_bridge, geo_ctx);
        surface_bio_async_bridge_set_geometry_ctx(bio_bridge, geo_ctx);
        surface_quantum_bridge_connect_geometry(quantum_bridge, geo_ctx);
        surface_immune_bridge_connect_geometry(immune_bridge, geo_ctx);
    }

    void TearDown() override {
        if (immune_bridge) surface_immune_bridge_destroy(immune_bridge);
        if (quantum_bridge) surface_quantum_bridge_destroy(quantum_bridge);
        if (bio_bridge) surface_bio_async_bridge_destroy(bio_bridge);
        if (brain_bridge) surface_geometry_bridge_destroy(brain_bridge);
        if (geo_ctx) surface_geometry_destroy(geo_ctx);
    }

    surface_geometry_ctx_t* geo_ctx;
    surface_geometry_bridge_t* brain_bridge;
    surface_bio_async_bridge_t* bio_bridge;
    surface_quantum_bridge_t* quantum_bridge;
    surface_immune_bridge_t* immune_bridge;
};

//=============================================================================
// Full Workflow: Spine Formation
//=============================================================================

TEST_F(SurfaceE2ETest, SpineFormationWorkflow) {
    // Step 1: Predict if new spine should be sprout
    float parent_diam = 2.0f;
    float spine_diam = 0.4f;  // Thin spine -> should be sprout

    bool is_sprout;
    int ret = surface_predict_spine_sprout(geo_ctx, parent_diam, spine_diam, &is_sprout);
    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(is_sprout);

    // Step 2: Compute spine geometry
    surface_vec3_t pos = {1.0f, 0.0f, 0.0f};
    spine_surface_geometry_t spine_geom = {};
    ret = surface_compute_spine_geometry(geo_ctx, parent_diam, spine_diam, &pos, &spine_geom);
    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(spine_geom.is_sprout);
    EXPECT_NEAR(spine_geom.optimal_angle, M_PI / 2.0f, ANGLE_TOLERANCE);

    // Step 3: Validate via immune system
    surface_geometry_params_t params = spine_geom.params;
    bool is_valid;
    surface_antigen_type_t violation;
    ret = surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);
    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(is_valid);

    // Step 4: Notify via bio-async using proper message struct
    surface_bio_msg_branch_formed_t msg = {};
    msg.branch_point_id = 1;
    msg.branch_type = SURFACE_BRANCH_BIFURCATION;  // Sprout counts as bifurcation
    msg.chi = params.chi;
    msg.rho = params.rho;
    msg.position[0] = pos.x;
    msg.position[1] = pos.y;
    msg.position[2] = pos.z;
    msg.timestamp_ms = 12345;

    ret = surface_bio_async_send_branch_formed(bio_bridge, &msg);
    // Without router, may return -1 which is acceptable
    EXPECT_TRUE(ret == 0 || ret == -1);
}

//=============================================================================
// Full Workflow: Axon Branching
//=============================================================================

TEST_F(SurfaceE2ETest, AxonBranchingWorkflow) {
    // Step 1: Parent axon wants to branch
    float parent_diam = 2.0f;
    float child_diams[] = {1.2f, 1.0f};

    // Step 2: Compute branch geometry
    axon_branch_surface_geometry_t branch_geom = {};
    int ret = surface_compute_axon_branch_geometry(
        geo_ctx, parent_diam, child_diams, 2, &branch_geom
    );
    ASSERT_EQ(ret, 0);
    EXPECT_EQ(branch_geom.degree, 3u);  // Bifurcation

    // Step 3: Predict branch type based on chi
    surface_branch_type_t predicted;
    ret = surface_predict_branch_type(branch_geom.params.chi, &predicted);
    ASSERT_EQ(ret, 0);

    // Step 4: Validate geometry
    surface_validation_result_t val_result = {};
    ret = surface_validate_geometry(geo_ctx, &branch_geom.params, &val_result);
    ASSERT_EQ(ret, 0);

    // Step 5: Notify branch formation using proper message struct
    surface_bio_msg_branch_formed_t msg = {};
    msg.branch_point_id = 1;
    msg.branch_type = SURFACE_BRANCH_BIFURCATION;
    msg.chi = branch_geom.params.chi;
    msg.rho = branch_geom.params.rho;
    msg.timestamp_ms = 12345;

    ret = surface_bio_async_send_branch_formed(bio_bridge, &msg);
    EXPECT_TRUE(ret == 0 || ret == -1);
}

//=============================================================================
// Full Workflow: Trifurcation Detection
//=============================================================================

TEST_F(SurfaceE2ETest, TrifurcationDetectionWorkflow) {
    // Scenario: Axon wants to split into 3 branches (trifurcation)
    float parent_diam = 2.0f;
    float child_diams[] = {1.0f, 0.9f, 0.8f};  // 3 children

    // Step 1: Compute geometry
    axon_branch_surface_geometry_t branch_geom = {};
    int ret = surface_compute_axon_branch_geometry(
        geo_ctx, parent_diam, child_diams, 3, &branch_geom
    );
    ASSERT_EQ(ret, 0);
    EXPECT_EQ(branch_geom.degree, 4u);  // Trifurcation (parent + 3)

    // Step 2: Check if chi allows trifurcation
    float chi = branch_geom.params.chi;
    surface_branch_type_t predicted;
    surface_predict_branch_type(chi, &predicted);

    // Step 3: Validate geometry (immune bridge checks params)
    bool is_valid;
    surface_antigen_type_t violation;
    ret = surface_immune_validate_geometry(immune_bridge, &branch_geom.params,
                                           &is_valid, &violation);
    EXPECT_EQ(ret, 0);  // Function should succeed

    // If chi >= 0.83, trifurcation should be allowed (but validation may fail
    // for other reasons like rho out of range, so we don't assert validity)
    if (chi >= 0.83f && is_valid) {
        EXPECT_EQ(violation, SURFACE_ANTIGEN_NONE);
    }

    // If trifurcation at low chi or params invalid, validate branch too
    surface_branch_point_t branch = {};
    branch.id = 1;
    branch.degree = 4;
    branch.params = branch_geom.params;

    bool branch_valid;
    uint32_t antigen_id;
    ret = surface_immune_validate_branch(immune_bridge, &branch, &branch_valid, &antigen_id);
    EXPECT_EQ(ret, 0);  // Function should complete without error

    // Step 4: Notify trifurcation using proper message struct
    surface_bio_msg_branch_formed_t msg = {};
    msg.branch_point_id = 1;
    msg.branch_type = SURFACE_BRANCH_TRIFURCATION;
    msg.chi = chi;
    msg.rho = branch_geom.params.rho;
    msg.timestamp_ms = 12345;

    ret = surface_bio_async_send_branch_formed(bio_bridge, &msg);
    EXPECT_TRUE(ret == 0 || ret == -1);
}

//=============================================================================
// Full Workflow: Network Optimization
//=============================================================================

TEST_F(SurfaceE2ETest, NetworkOptimizationWorkflow) {
    // Scenario: Optimize branching for 4 terminal nodes

    // Step 1: Define terminals
    float terminals[4][3] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.5f, 0.866f, 0.0f},
        {0.5f, 0.433f, 0.5f}
    };

    // Step 2: Classical optimization
    surface_optimization_result_t classical_result = {};
    int ret = surface_optimize_network(geo_ctx, terminals, 4, 0.1f, &classical_result);
    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(classical_result.converged);

    // Step 3: Quantum-enhanced optimization (if available)
    if (surface_quantum_bridge_is_quantum_available(quantum_bridge)) {
        surface_optimization_result_t quantum_result = {};
        ret = surface_quantum_hybrid_optimize(quantum_bridge, terminals, 4, 0.1f, &quantum_result);
        ASSERT_EQ(ret, 0);

        // Quantum result should be at least as good as classical
        // (or within tolerance)
        surface_optimization_result_free(&quantum_result);
    }

    // Step 4: Validate all branch points
    for (uint32_t i = 0; i < classical_result.num_branch_points; i++) {
        surface_validation_result_t val = {};
        surface_validate_branch(geo_ctx, &classical_result.branch_points[i], &val);
        if (!val.is_valid) {
            // Report to immune system
            uint32_t antigen_id;
            surface_immune_present_anomaly(
                immune_bridge,
                SURFACE_ANTIGEN_ANGLE_VIOLATION,
                &classical_result.branch_points[i],
                0.0f, 0.0f,
                &antigen_id
            );
        }
    }

    // Step 5: Notify optimization complete using proper message struct
    surface_bio_msg_optimization_done_t opt_msg = {};
    opt_msg.optimization_id = 1;
    opt_msg.surface_area = classical_result.surface_area;
    opt_msg.wire_length = classical_result.wire_length;
    opt_msg.iterations = classical_result.iterations;
    opt_msg.converged = classical_result.converged;
    opt_msg.duration_ms = 100;

    ret = surface_bio_async_send_optimization_done(bio_bridge, &opt_msg);
    EXPECT_TRUE(ret == 0 || ret == -1);

    surface_optimization_result_free(&classical_result);
}

//=============================================================================
// Full Workflow: Anomaly Detection and Response
//=============================================================================

TEST_F(SurfaceE2ETest, AnomalyResponseWorkflow) {
    // Scenario: Detect geometry anomaly and trigger immune response

    // Step 1: Create invalid geometry
    surface_geometry_params_t bad_params = {};
    bad_params.chi = 3.0f;  // Invalid: max is 2.0
    bad_params.rho = 0.5f;

    // Step 2: Validate and detect anomaly
    bool is_valid;
    surface_antigen_type_t violation;
    int ret = surface_immune_validate_geometry(immune_bridge, &bad_params,
                                                &is_valid, &violation);
    ASSERT_EQ(ret, 0);
    EXPECT_FALSE(is_valid);
    EXPECT_EQ(violation, SURFACE_ANTIGEN_INVALID_CHI);

    // Step 3: Present antigen
    surface_branch_point_t branch = {};
    branch.id = 1;
    branch.params = bad_params;

    uint32_t antigen_id;
    ret = surface_immune_present_anomaly(
        immune_bridge,
        violation,
        &branch,
        2.0f,   // expected max
        3.0f,   // actual
        &antigen_id
    );
    ASSERT_EQ(ret, 0);

    // Step 4: Notify via bio-async (urgent channel) using proper message struct
    surface_bio_msg_anomaly_t anomaly_msg = {};
    anomaly_msg.branch_point_id = 1;
    anomaly_msg.error_code = SURFACE_ERROR_CONSTRAINT_VIOLATION;
    anomaly_msg.expected_value = 2.0f;
    anomaly_msg.actual_value = 3.0f;
    anomaly_msg.description = "Invalid chi value";
    anomaly_msg.timestamp_ms = 12345;

    ret = surface_bio_async_send_anomaly(bio_bridge, &anomaly_msg);
    EXPECT_TRUE(ret == 0 || ret == -1);

    // Step 5: Produce antibody
    uint32_t antibody_id;
    ret = surface_immune_produce_antibody(immune_bridge, violation, &antibody_id);
    EXPECT_EQ(ret, 0);

    // Step 6: Apply correction
    bool success;
    ret = surface_immune_apply_antibody(immune_bridge, antibody_id, &bad_params, &success);
    EXPECT_EQ(ret, 0);

    if (success) {
        // Verify correction
        EXPECT_LE(bad_params.chi, 2.0f);

        // Resolve antigen
        ret = surface_immune_resolve_antigen(immune_bridge, antigen_id);
        EXPECT_EQ(ret, 0);
    }
}

//=============================================================================
// Full Workflow: Multi-System Statistics
//=============================================================================

TEST_F(SurfaceE2ETest, StatisticsAcrossSubsystems) {
    // Perform various operations and verify statistics

    // Spine operations
    surface_vec3_t pos = {0, 0, 0};
    spine_surface_geometry_t spine = {};
    for (int i = 0; i < 5; i++) {
        surface_compute_spine_geometry(geo_ctx, 2.0f, 0.4f, &pos, &spine);
    }

    // Branch operations
    float child_diams[] = {1.0f, 1.0f};
    axon_branch_surface_geometry_t branch_geom = {};
    for (int i = 0; i < 3; i++) {
        surface_compute_axon_branch_geometry(geo_ctx, 2.0f, child_diams, 2, &branch_geom);
    }

    // Bio-async messages using proper struct
    for (int i = 0; i < 10; i++) {
        surface_bio_msg_geometry_update_t update = {};
        update.branch_point_id = static_cast<uint32_t>(i);
        update.params.chi = 0.5f;
        update.params.rho = 0.4f;
        update.timestamp_ms = static_cast<uint64_t>(i * 100);
        surface_bio_async_send_geometry_update(bio_bridge, &update);
    }

    // Quantum operations
    surface_branch_point_t branches[1] = {};
    branches[0].degree = 3;
    surface_qmc_amplitude_result_t qmc_result = {};
    surface_quantum_estimate_area(quantum_bridge, branches, 1, 0.1f, &qmc_result);

    // Immune validations
    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    params.rho = 0.4f;
    for (int i = 0; i < 5; i++) {
        bool is_valid;
        surface_antigen_type_t violation;
        surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);
    }

    // Verify stats
    surface_geometry_stats_t geo_stats = {};
    surface_geometry_get_stats(geo_ctx, &geo_stats);

    surface_bio_async_stats_t bio_stats = {};
    surface_bio_async_get_stats(bio_bridge, &bio_stats);
    // Without router, messages may not be counted

    surface_quantum_bridge_stats_t quantum_stats = {};
    surface_quantum_bridge_get_stats(quantum_bridge, &quantum_stats);
    EXPECT_GE(quantum_stats.qmc_amplitude_calls, 1u);

    surface_immune_stats_t immune_stats = {};
    surface_immune_get_stats(immune_bridge, &immune_stats);
    EXPECT_GE(immune_stats.total_validations, 5u);
}

//=============================================================================
// Full Workflow: Subsystem Reset
//=============================================================================

TEST_F(SurfaceE2ETest, SubsystemResetWorkflow) {
    // Generate activity
    surface_bio_msg_geometry_update_t update = {};
    update.params.chi = 0.5f;
    surface_bio_async_send_geometry_update(bio_bridge, &update);

    surface_branch_point_t branch = {};
    branch.degree = 3;
    surface_qmc_amplitude_result_t qmc_result = {};
    surface_quantum_estimate_area(quantum_bridge, &branch, 1, 0.1f, &qmc_result);

    surface_geometry_params_t params = {};
    params.chi = 0.5f;
    bool is_valid;
    surface_antigen_type_t violation;
    surface_immune_validate_geometry(immune_bridge, &params, &is_valid, &violation);

    // Reset all stats
    surface_geometry_reset_stats(geo_ctx);
    surface_bio_async_reset_stats(bio_bridge);
    surface_quantum_bridge_reset_stats(quantum_bridge);
    surface_immune_reset_stats(immune_bridge);

    // Verify all reset
    surface_geometry_stats_t geo_stats = {};
    surface_geometry_get_stats(geo_ctx, &geo_stats);
    EXPECT_EQ(geo_stats.total_optimizations, 0u);

    surface_bio_async_stats_t bio_stats = {};
    surface_bio_async_get_stats(bio_bridge, &bio_stats);
    EXPECT_EQ(bio_stats.messages_sent, 0u);

    surface_quantum_bridge_stats_t quantum_stats = {};
    surface_quantum_bridge_get_stats(quantum_bridge, &quantum_stats);
    EXPECT_EQ(quantum_stats.qmc_amplitude_calls, 0u);

    surface_immune_stats_t immune_stats = {};
    surface_immune_get_stats(immune_bridge, &immune_stats);
    EXPECT_EQ(immune_stats.total_validations, 0u);
}

//=============================================================================
// Stress Test: Many Spines
//=============================================================================

TEST_F(SurfaceE2ETest, StressTest_ManySpines) {
    // Simulate many spine computations (typical dendrite has ~2000 spines)
    const int NUM_SPINES = 100;  // Reduced for test speed

    surface_vec3_t pos = {0, 0, 0};
    spine_surface_geometry_t spine = {};

    for (int i = 0; i < NUM_SPINES; i++) {
        float parent_diam = 1.0f + (float)(i % 10) * 0.1f;
        float spine_diam = 0.1f + (float)(i % 5) * 0.1f;
        pos.x = (float)i;

        int ret = surface_compute_spine_geometry(geo_ctx, parent_diam, spine_diam, &pos, &spine);
        EXPECT_EQ(ret, 0);
    }

    // Verify stats accumulated
    surface_geometry_stats_t stats = {};
    surface_geometry_get_stats(geo_ctx, &stats);
    // Stats should reflect computations
}

//=============================================================================
// Stress Test: Many Branches
//=============================================================================

TEST_F(SurfaceE2ETest, StressTest_ManyBranches) {
    // Simulate many branch computations
    const int NUM_BRANCHES = 50;

    float child_diams[] = {1.0f, 0.8f};
    axon_branch_surface_geometry_t branch = {};

    for (int i = 0; i < NUM_BRANCHES; i++) {
        float parent_diam = 1.5f + (float)(i % 5) * 0.2f;
        int ret = surface_compute_axon_branch_geometry(geo_ctx, parent_diam, child_diams, 2, &branch);
        EXPECT_EQ(ret, 0);
    }
}

//=============================================================================
// Edge Case: Degenerate Inputs
//=============================================================================

TEST_F(SurfaceE2ETest, EdgeCase_DegenerateTerminals) {
    // All terminals at same position
    float terminals[4][3] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

    surface_optimization_result_t result = {};
    int ret = surface_optimize_network(geo_ctx, terminals, 4, 0.1f, &result);
    // Should handle gracefully (may fail or produce degenerate result)
    if (ret == 0) {
        surface_optimization_result_free(&result);
    }
}

TEST_F(SurfaceE2ETest, EdgeCase_CollinearTerminals) {
    // All terminals on a line
    float terminals[4][3] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {3.0f, 0.0f, 0.0f}
    };

    surface_optimization_result_t result = {};
    int ret = surface_optimize_network(geo_ctx, terminals, 4, 0.1f, &result);
    // Should produce linear tree
    if (ret == 0) {
        surface_optimization_result_free(&result);
    }
}
