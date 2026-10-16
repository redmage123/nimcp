/**
 * @file test_wave15_physics.cpp
 * @brief Unit test for KG-integration Wave W15 (physics runtime events).
 *
 * W15 wires runtime event emission + read path across 8 physics modules:
 *   PARTIAL → full (already had structural write at init):
 *     1. src/physics/bridges/nimcp_physics_kg_wiring.c
 *        — physics_kg_trigger_state_summary + physics_kg_count_events
 *     2. src/physics/graphs/nimcp_graph_theory_bridge.c
 *        — graph_theory_kg_trigger_metric_event + count
 *     3. src/physics/dynamics/nimcp_dynamical_systems.c
 *        — dynsys_kg_trigger_bifurcation_event + count
 *     4. src/physics/geometry/nimcp_information_geometry_bridge.c
 *        — info_geom_bridge_kg_trigger_manifold_event + count
 *
 *   UNWIRED → full (aggregator-level emit, never per-neuron/per-tick):
 *     5. src/physics/biophysics/nimcp_hodgkin_huxley.c
 *        — nimcp_hh_kg_trigger_mode_event (rate-limited by spike batch)
 *     6. src/physics/ephaptic/nimcp_ephaptic.c
 *        — nimcp_ephaptic_kg_trigger_field_event (LFP/phase aggregated)
 *     7. src/physics/thermodynamics/nimcp_thermodynamics.c
 *        — nimcp_thermo_kg_trigger_dissipation_event
 *     8. src/physics/geometry/nimcp_information_geometry.c
 *        — nimcp_info_geom_kg_trigger_manifold_event (core, not bridge)
 *
 * Naming rules per docs/claude/kg-node-naming-registry.md §4:
 *     <owner>_event_<kind>_<ts_us>
 * Admin-token elevation per §7 (bridges that only take brain_kg_t* do their
 * own elevation via the backpointer).
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

/* nimcp_bio_router_t pre-existing typedef conflict: hh declares
 *   typedef struct nimcp_bio_router_s  nimcp_bio_router_t;
 * info_geom declares
 *   typedef struct nimcp_bio_router_struct* nimcp_bio_router_t;
 * Both are incompatible. Our test forward-declares the W15 APIs from
 * hh/ephaptic/thermo without pulling those headers in at all, while the
 * info_geom_bridge header (and info_geom core) is included normally.
 * See docs/claude/kg-integration-audit-2026-04-24.md risk 6. */
#include "physics/bridges/nimcp_physics_kg_wiring.h"
#include "physics/graphs/nimcp_graph_theory_bridge.h"
#include "physics/dynamics/nimcp_dynamical_systems.h"
#include "physics/geometry/nimcp_information_geometry_bridge.h"
#include "physics/geometry/nimcp_information_geometry.h"

/* Forward-declare the W15 APIs from hh / ephaptic / thermo to avoid
 * pulling their full headers and hitting the nimcp_bio_router_t conflict. */
extern "C" {
    /* HH */
    void nimcp_hh_kg_register_brain(brain_t brain);
    void nimcp_hh_kg_trigger_mode_event(brain_t brain, const char* kind,
        float firing_rate, uint32_t spike_count, uint64_t ts_us);
    uint32_t nimcp_hh_kg_count_events(struct brain_kg* kg, const char* substr);

    /* Ephaptic */
    void nimcp_ephaptic_kg_register_brain(brain_t brain);
    void nimcp_ephaptic_kg_trigger_field_event(brain_t brain, const char* kind,
        float lfp_amplitude_mv, float phase_coherence, uint64_t ts_us);
    uint32_t nimcp_ephaptic_kg_count_events(struct brain_kg* kg, const char* substr);

    /* Thermodynamics */
    void nimcp_thermo_kg_register_brain(brain_t brain);
    void nimcp_thermo_kg_trigger_dissipation_event(brain_t brain, const char* kind,
        double entropy_rate, double atp_ratio, double power_consumption_w,
        uint64_t ts_us);
    uint32_t nimcp_thermo_kg_count_events(struct brain_kg* kg, const char* substr);
}

/* ------------------------------------------------------------------------- */
class Wave15PhysicsKgTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave15_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr);
        ASSERT_TRUE(brain->internal_kg_enabled);
        ASSERT_NE(brain->internal_kg, nullptr);
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    void elevate_to_admin() {
        brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_ADMIN,
                                  brain->internal_kg_admin_token);
    }
    void restore_to_read() {
        brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
    }

    uint32_t count_nodes_with_substring(const char* substr) {
        /* Walk brain_kg_stats would need stats API; simpler: iterate by node
         * type. We just use brain_kg_find_node with the exact event name and
         * check non-null. For tests we keep it simple and probe by exact name. */
        (void)substr;
        return 0;
    }
};

/* ------------------------------------------------------------------------- */
/* Test 1: physics_kg_wiring.c — state summary emit + count                   */
/* ------------------------------------------------------------------------- */

TEST_F(Wave15PhysicsKgTest, PhysicsKgStateSummaryEmitsEventAndCountsRead) {
    /* First register_all to ensure the physics_layer root exists. */
    elevate_to_admin();
    physics_kg_state_t state;
    std::memset(&state, 0, sizeof(state));
    int rc = physics_kg_register_all(brain->internal_kg, nullptr, &state,
                                     brain->internal_kg_admin_token);
    EXPECT_EQ(rc, 0);
    restore_to_read();

    ASSERT_NE(brain_kg_find_node(brain->internal_kg, "physics_layer"),
              BRAIN_KG_INVALID_NODE);

    /* Now emit a runtime summary (this self-elevates internally). */
    physics_kg_trigger_state_summary(brain, 310.0f, 0.85f, 0.02f,
                                     "epoch_boundary", 1000001ull);
    physics_kg_trigger_state_summary(brain, 311.0f, 0.80f, 0.03f,
                                     "mode_change", 1000002ull);

    /* The event node should now exist. */
    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "physics_event_state_summary_1000001"), BRAIN_KG_INVALID_NODE);
    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "physics_event_state_summary_1000002"), BRAIN_KG_INVALID_NODE);

    /* Read-path: count_events with substring match. */
    uint32_t n = physics_kg_count_events(brain->internal_kg, "state_summary");
    EXPECT_GE(n, 2u);
}

/* ------------------------------------------------------------------------- */
/* Test 2: graph_theory_bridge.c — metric event emit + count                  */
/* ------------------------------------------------------------------------- */

TEST_F(Wave15PhysicsKgTest, GraphTheoryMetricEventEmitsAndCounts) {
    graph_theory_bridge_t bridge = graph_theory_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    elevate_to_admin();
    graph_theory_error_t err = graph_theory_bridge_register_kg(
        bridge, brain->internal_kg, brain->internal_kg_admin_token);
    EXPECT_EQ(err, GRAPH_THEORY_OK);
    restore_to_read();

    graph_theory_kg_register_brain(bridge, brain);

    graph_theory_kg_trigger_metric_event(bridge, "modularity_shift",
                                         0.42f, 2000001ull);
    graph_theory_kg_trigger_metric_event(bridge, "clustering_drop",
                                         0.17f, 2000002ull);

    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "graph_theory_event_modularity_shift_2000001"),
              BRAIN_KG_INVALID_NODE);

    uint32_t cnt = graph_theory_kg_count_metric_events(bridge, "modularity");
    EXPECT_GE(cnt, 1u);

    graph_theory_bridge_destroy(bridge);
}

/* ------------------------------------------------------------------------- */
/* Test 3: dynamical_systems.c — bifurcation event emit + count               */
/* ------------------------------------------------------------------------- */

/* Dummy ODE for dynsys test system creation (constant zero-rate). */
static int wave15_dummy_dynsys_func(const float* state, uint32_t state_dim,
                                     const float* params, uint32_t param_dim,
                                     float* derivative, void* ctx) {
    (void)state; (void)params; (void)param_dim; (void)ctx;
    if (derivative) {
        for (uint32_t i = 0; i < state_dim; i++) derivative[i] = 0.0f;
    }
    return 0;
}

TEST_F(Wave15PhysicsKgTest, DynSysBifurcationEventEmitsAndCounts) {
    dynsys_config_t sys_cfg = dynsys_default_config();
    dynsys_system_t sys = dynsys_create(&sys_cfg, wave15_dummy_dynsys_func,
                                        nullptr, nullptr);
    ASSERT_NE(sys, nullptr);

    dynsys_bridge_config_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    cfg.enable_kg_wiring = true;
    cfg.enable_logging = false;
    dynsys_bridge_t bridge = dynsys_bridge_create(&cfg, sys);
    ASSERT_NE(bridge, nullptr);

    elevate_to_admin();
    int rc = dynsys_bridge_register_kg(bridge, brain->internal_kg);
    EXPECT_EQ(rc, 0);
    restore_to_read();

    dynsys_kg_register_brain(bridge, brain);

    dynsys_kg_trigger_bifurcation_event(bridge, "bifurcation_detected",
                                        3.2f, 0.18f, 3000001ull);
    dynsys_kg_trigger_bifurcation_event(bridge, "chaos_onset",
                                        3.9f, 0.85f, 3000002ull);

    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "dynamical_systems_event_bifurcation_detected_3000001"),
              BRAIN_KG_INVALID_NODE);
    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "dynamical_systems_event_chaos_onset_3000002"),
              BRAIN_KG_INVALID_NODE);

    uint32_t cnt = dynsys_kg_count_events(bridge, "chaos");
    EXPECT_GE(cnt, 1u);

    dynsys_bridge_destroy(bridge);
    dynsys_destroy(sys);
}

/* ------------------------------------------------------------------------- */
/* Test 4: information_geometry_bridge.c — manifold event emit + count        */
/* ------------------------------------------------------------------------- */

TEST_F(Wave15PhysicsKgTest, InfoGeomBridgeManifoldEventEmitsAndCounts) {
    info_geom_bridge_config_t cfg = info_geom_bridge_default_config();
    info_geom_bridge_t bridge = info_geom_bridge_create(&cfg);
    ASSERT_NE(bridge, nullptr);

    elevate_to_admin();
    int rc = info_geom_bridge_register_kg(bridge, brain->internal_kg);
    EXPECT_EQ(rc, 0);
    restore_to_read();

    info_geom_bridge_kg_register_brain(bridge, brain);

    info_geom_bridge_kg_trigger_manifold_event(bridge, "manifold_shift",
                                               -0.25f, 0.031f, 4000001ull);
    info_geom_bridge_kg_trigger_manifold_event(bridge, "curvature_spike",
                                               0.85f, 0.120f, 4000002ull);

    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "information_geometry_event_manifold_shift_4000001"),
              BRAIN_KG_INVALID_NODE);

    uint32_t cnt = info_geom_bridge_kg_count_events(bridge, "curvature");
    EXPECT_GE(cnt, 1u);

    info_geom_bridge_destroy(bridge);
}

/* ------------------------------------------------------------------------- */
/* Test 5: hodgkin_huxley.c — rate-limited mode event                         */
/* ------------------------------------------------------------------------- */

TEST_F(Wave15PhysicsKgTest, HhRateLimitedModeEvent) {
    nimcp_hh_kg_register_brain(brain);

    /* Rate limit: non-mode-transition kind with spike_count < 100 must be
     * dropped. "rate_sample" is not a mode transition. */
    nimcp_hh_kg_trigger_mode_event(brain, "rate_sample", 5.0f, 10, 5000001ull);
    EXPECT_EQ(brain_kg_find_node(brain->internal_kg,
              "hodgkin_huxley_event_rate_sample_5000001"),
              BRAIN_KG_INVALID_NODE)
        << "Below-threshold rate_sample must NOT emit (rate-limiter check)";

    /* Mode-transition kind: always emitted regardless of count. */
    nimcp_hh_kg_trigger_mode_event(brain, "mode_tonic", 12.5f, 0, 5000002ull);
    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "hodgkin_huxley_event_mode_tonic_5000002"),
              BRAIN_KG_INVALID_NODE)
        << "mode_tonic is a mode transition and must emit unconditionally";

    /* Above-threshold non-mode kind emits. */
    nimcp_hh_kg_trigger_mode_event(brain, "rate_spike", 80.0f, 200, 5000003ull);
    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "hodgkin_huxley_event_rate_spike_5000003"),
              BRAIN_KG_INVALID_NODE);

    /* Read-path. */
    uint32_t cnt = nimcp_hh_kg_count_events(
        (struct brain_kg*)brain->internal_kg, "mode_tonic");
    EXPECT_GE(cnt, 1u);
}

/* ------------------------------------------------------------------------- */
/* Test 6: ephaptic.c — aggregated field event                                */
/* ------------------------------------------------------------------------- */

TEST_F(Wave15PhysicsKgTest, EphapticFieldEventEmitsAndCounts) {
    nimcp_ephaptic_kg_register_brain(brain);

    nimcp_ephaptic_kg_trigger_field_event(brain, "field_sync",
                                          0.15f, 0.87f, 6000001ull);
    nimcp_ephaptic_kg_trigger_field_event(brain, "phase_lock",
                                          0.12f, 0.93f, 6000002ull);

    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "ephaptic_event_field_sync_6000001"),
              BRAIN_KG_INVALID_NODE);

    uint32_t cnt = nimcp_ephaptic_kg_count_events(
        (struct brain_kg*)brain->internal_kg, "phase_lock");
    EXPECT_GE(cnt, 1u);
}

/* ------------------------------------------------------------------------- */
/* Test 7: thermodynamics.c — aggregated dissipation event                    */
/* ------------------------------------------------------------------------- */

TEST_F(Wave15PhysicsKgTest, ThermoDissipationEventEmitsAndCounts) {
    nimcp_thermo_kg_register_brain(brain);

    nimcp_thermo_kg_trigger_dissipation_event(brain, "atp_critical",
                                              1.5e-8, 0.05, 20.0, 7000001ull);
    nimcp_thermo_kg_trigger_dissipation_event(brain, "entropy_spike",
                                              3.2e-7, 0.60, 22.0, 7000002ull);

    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "thermodynamics_event_atp_critical_7000001"),
              BRAIN_KG_INVALID_NODE);

    uint32_t cnt = nimcp_thermo_kg_count_events(
        (struct brain_kg*)brain->internal_kg, "entropy");
    EXPECT_GE(cnt, 1u);
}

/* ------------------------------------------------------------------------- */
/* Test 8: information_geometry.c (core, not bridge) — manifold event         */
/* ------------------------------------------------------------------------- */

TEST_F(Wave15PhysicsKgTest, InfoGeomCoreManifoldEventEmitsAndCounts) {
    nimcp_info_geom_kg_register_brain(brain);

    nimcp_info_geom_kg_trigger_manifold_event(brain, "speedup_shift",
                                              4.2f, 0.015f, 8000001ull);
    nimcp_info_geom_kg_trigger_manifold_event(brain, "embedding_converged",
                                              1.1f, 0.008f, 8000002ull);

    EXPECT_NE(brain_kg_find_node(brain->internal_kg,
              "information_geometry_runtime_event_speedup_shift_8000001"),
              BRAIN_KG_INVALID_NODE);

    uint32_t cnt = nimcp_info_geom_kg_count_events(
        (struct brain_kg*)brain->internal_kg, "embedding_converged");
    EXPECT_GE(cnt, 1u);
}
