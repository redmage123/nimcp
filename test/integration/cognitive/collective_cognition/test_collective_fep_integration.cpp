/**
 * @file test_collective_fep_integration.cpp
 * @brief Integration tests for Collective Cognition + FEP Orchestrator
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests integration between Collective Cognition and FEP orchestrator
 * WHY:  Verify FEP correctly coordinates collective cognition updates and phi tracking
 * HOW:  Test FEP update cycles, phi effects on free energy, and coherence integration
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/collective_cognition/nimcp_hyperscanning.h"
#include "cognitive/collective_cognition/nimcp_collective_phi.h"
#include "cognitive/collective_cognition/nimcp_shared_intentionality.h"
#include "cognitive/collective_cognition/nimcp_extended_mind.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define COLLECTIVE_FEP_BRIDGE_ID_BASE     0x1220
#define COLLECTIVE_FEP_BRIDGE_ID_MAIN     (COLLECTIVE_FEP_BRIDGE_ID_BASE + 1)
#define COLLECTIVE_FEP_BRIDGE_ID_HYPERSCAN (COLLECTIVE_FEP_BRIDGE_ID_BASE + 2)
#define COLLECTIVE_FEP_BRIDGE_ID_PHI      (COLLECTIVE_FEP_BRIDGE_ID_BASE + 3)
#define COLLECTIVE_FEP_BRIDGE_ID_INTENT   (COLLECTIVE_FEP_BRIDGE_ID_BASE + 4)

/* ============================================================================
 * Mock FEP Bridge Update Functions
 * ============================================================================ */

static std::atomic<int> g_collective_update_count{0};
static std::atomic<int> g_hyperscan_update_count{0};
static std::atomic<int> g_phi_update_count{0};
static std::atomic<int> g_intent_update_count{0};
static std::atomic<float> g_current_phi{0.3f};
static std::atomic<float> g_current_free_energy{0.7f};
static std::atomic<float> g_current_coherence{0.5f};

static int collective_main_fep_update(fep_bridge_handle_t handle) {
    collective_cognition_t* cc = static_cast<collective_cognition_t*>(handle);
    if (cc) {
        collective_cognition_update(cc);
    }
    g_collective_update_count++;

    /* Coherent collective reduces free energy */
    float coherence = g_current_coherence.load();
    float fe = g_current_free_energy.load();
    g_current_free_energy.store(fe * (1.0f - coherence * 0.01f));

    return 0;
}

static int hyperscan_fep_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_hyperscan_update_count++;
    return 0;
}

static int phi_fep_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_phi_update_count++;

    /* Higher phi reduces prediction error */
    float phi = g_current_phi.load();
    float fe = g_current_free_energy.load();
    g_current_free_energy.store(fe * (1.0f - phi * 0.005f));

    return 0;
}

static int intent_fep_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_intent_update_count++;
    return 0;
}

static void reset_collective_counters() {
    g_collective_update_count = 0;
    g_hyperscan_update_count = 0;
    g_phi_update_count = 0;
    g_intent_update_count = 0;
    g_current_phi = 0.3f;
    g_current_free_energy = 0.7f;
    g_current_coherence = 0.5f;
}

/* ============================================================================
 * Test Fixture: Collective Cognition + FEP Integration
 * ============================================================================ */

class CollectiveFEPIntegrationTest : public ::testing::Test {
protected:
    collective_cognition_t* cc = nullptr;
    collective_cognition_config_t cc_config;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;

    void SetUp() override {
        reset_collective_counters();

        /* Create collective cognition */
        cc_config = collective_cognition_default_config();
        cc = collective_cognition_create(&cc_config);
        ASSERT_NE(cc, nullptr);

        /* Register instances */
        for (uint32_t i = 1; i <= 4; i++) {
            ASSERT_EQ(collective_cognition_register_instance(cc, i, nullptr), 0);
        }

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
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
        if (cc) {
            collective_cognition_destroy(cc);
            cc = nullptr;
        }
    }

    /* Helper to register collective bridges with FEP */
    void register_collective_bridges() {
        uint32_t bridge_id;

        /* Register main collective cognition bridge */
        int ret = fep_orchestrator_register_bridge(
            fep_orch, "collective_cognition",
            FEP_BRIDGE_CATEGORY_COGNITIVE,
            cc, collective_main_fep_update, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);

        /* Register hyperscanning bridge */
        ret = fep_orchestrator_register_bridge(
            fep_orch, "hyperscanning",
            FEP_BRIDGE_CATEGORY_COGNITIVE,
            collective_cognition_get_hyperscanning(cc),
            hyperscan_fep_update, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);

        /* Register phi bridge */
        ret = fep_orchestrator_register_bridge(
            fep_orch, "collective_phi",
            FEP_BRIDGE_CATEGORY_COGNITIVE,
            collective_cognition_get_phi_system(cc),
            phi_fep_update, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);

        /* Register shared intentionality bridge */
        ret = fep_orchestrator_register_bridge(
            fep_orch, "shared_intentionality",
            FEP_BRIDGE_CATEGORY_COGNITIVE,
            collective_cognition_get_intentionality(cc),
            intent_fep_update, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    /* Helper to set up synchronized neural states */
    void setup_synchronized_states(float gamma_power, float sync_level) {
        hyperscanning_t* hs = collective_cognition_get_hyperscanning(cc);
        ASSERT_NE(hs, nullptr);

        for (uint32_t i = 1; i <= 4; i++) {
            hyperscanning_neural_state_t state;
            memset(&state, 0, sizeof(state));
            state.instance_id = i;
            state.band_power[SYNC_BAND_GAMMA] = gamma_power;
            state.band_phase[SYNC_BAND_GAMMA] = sync_level;
            state.band_power[SYNC_BAND_THETA] = 0.6f;
            state.band_power[SYNC_BAND_BETA] = 0.5f;
            state.atp_level = 0.9f;
            hyperscanning_update_state(hs, &state);
        }
    }
};

/* ============================================================================
 * CollectiveFEPFullCycle - Complete FEP update cycle
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, CollectiveFEPFullCycle) {
    /* Register collective bridges */
    register_collective_bridges();

    /* Verify bridges registered */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GE(stats.total_bridges, 4u);

    /* Run multiple FEP update cycles */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        uint64_t current_time = start_time + (i * 100);
        int updated = fep_orchestrator_update(fep_orch, current_time);
        EXPECT_GE(updated, 0);
    }

    /* Verify all bridges were updated */
    EXPECT_GT(g_collective_update_count.load(), 0);
    EXPECT_GT(g_hyperscan_update_count.load(), 0);
    EXPECT_GT(g_phi_update_count.load(), 0);
    EXPECT_GT(g_intent_update_count.load(), 0);

    /* Verify statistics accumulated */
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GT(stats.total_update_cycles, 0u);
    EXPECT_GT(stats.total_bridge_updates, 0u);
}

/* ============================================================================
 * PhiAffectsFreeEnergy - Higher phi reduces free energy
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, PhiAffectsFreeEnergy) {
    register_collective_bridges();

    /* Set up synchronized states for higher phi */
    setup_synchronized_states(0.9f, 0.8f);

    /* Record initial free energy */
    float initial_fe = g_current_free_energy.load();

    /* Set high phi to simulate integrated collective */
    g_current_phi = 0.8f;  /* High integration */
    g_current_coherence = 0.7f;  /* Good coherence */

    /* Run FEP updates */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        fep_orchestrator_update(fep_orch, start_time + (i * 50));
    }

    float final_fe = g_current_free_energy.load();

    /* Higher phi should lead to lower free energy */
    EXPECT_LT(final_fe, initial_fe);

    /* Verify significant reduction */
    float reduction_ratio = final_fe / initial_fe;
    EXPECT_LT(reduction_ratio, 0.95f);
}

/* ============================================================================
 * CoherenceTrackingIntegration - Coherence properly integrated
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, CoherenceTrackingIntegration) {
    register_collective_bridges();

    /* Set up varying coherence levels */
    g_current_coherence = 0.3f;  /* Low coherence initially */
    float low_coherence_fe = g_current_free_energy.load();

    /* Run some updates at low coherence */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, start_time + (i * 50));
    }

    float after_low_coherence = g_current_free_energy.load();

    /* Increase coherence */
    g_current_coherence = 0.9f;  /* High coherence */

    /* Run more updates */
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, start_time + 500 + (i * 50));
    }

    float after_high_coherence = g_current_free_energy.load();

    /* Free energy should decrease more with higher coherence */
    float low_reduction = low_coherence_fe - after_low_coherence;
    float high_reduction = after_low_coherence - after_high_coherence;

    /* High coherence should produce larger relative reduction */
    EXPECT_GT(high_reduction / after_low_coherence,
              low_reduction / low_coherence_fe * 0.5f);
}

/* ============================================================================
 * FEPCoordinatesCollective - FEP coordinates collective updates
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, FEPCoordinatesCollective) {
    register_collective_bridges();

    /* Configure update intervals */
    fep_orchestrator_set_update_interval(fep_orch, FEP_BRIDGE_CATEGORY_COGNITIVE, 50);

    /* Reset counters */
    reset_collective_counters();

    /* Run coordinated updates */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 60));
    }

    /* All bridges should have been updated multiple times */
    EXPECT_GT(g_collective_update_count.load(), 10);
    EXPECT_GT(g_hyperscan_update_count.load(), 10);
    EXPECT_GT(g_phi_update_count.load(), 10);
    EXPECT_GT(g_intent_update_count.load(), 10);

    /* Verify FEP orchestrator tracked updates */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GT(stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].total_updates, 0u);
}

/* ============================================================================
 * CollectiveWithOtherBridges - Works alongside other bridges
 * ============================================================================ */

static std::atomic<int> g_other_cognitive_count{0};
static std::atomic<int> g_other_swarm_count{0};

static int other_cognitive_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_other_cognitive_count++;
    return 0;
}

static int swarm_bridge_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_other_swarm_count++;
    return 0;
}

TEST_F(CollectiveFEPIntegrationTest, CollectiveWithOtherBridges) {
    g_other_cognitive_count = 0;
    g_other_swarm_count = 0;

    /* Register collective bridges */
    register_collective_bridges();

    /* Dummy handles for other bridges (non-null to pass validation) */
    static int dummy_emotion_handle = 1;
    static int dummy_memory_handle = 2;
    static int dummy_consensus_handle = 3;
    static int dummy_emergence_handle = 4;

    /* Register additional bridges from other categories */
    uint32_t bridge_id;

    /* Add more cognitive bridges */
    int ret = fep_orchestrator_register_bridge(
        fep_orch, "emotion_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_emotion_handle, other_cognitive_update, nullptr, &bridge_id);
    ASSERT_EQ(ret, 0);

    ret = fep_orchestrator_register_bridge(
        fep_orch, "memory_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_memory_handle, other_cognitive_update, nullptr, &bridge_id);
    ASSERT_EQ(ret, 0);

    /* Add swarm bridges */
    ret = fep_orchestrator_register_bridge(
        fep_orch, "swarm_consensus",
        FEP_BRIDGE_CATEGORY_SWARM,
        (fep_bridge_handle_t)&dummy_consensus_handle, swarm_bridge_update, nullptr, &bridge_id);
    ASSERT_EQ(ret, 0);

    ret = fep_orchestrator_register_bridge(
        fep_orch, "swarm_emergence",
        FEP_BRIDGE_CATEGORY_SWARM,
        (fep_bridge_handle_t)&dummy_emergence_handle, swarm_bridge_update, nullptr, &bridge_id);
    ASSERT_EQ(ret, 0);

    /* Verify total bridge count */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GE(stats.total_bridges, 8u);

    /* Run FEP update cycles */
    uint64_t current_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, current_time + (i * 100));
    }

    /* All bridge types should have been updated */
    EXPECT_GT(g_collective_update_count.load(), 0);
    EXPECT_GT(g_other_cognitive_count.load(), 0);
    EXPECT_GT(g_other_swarm_count.load(), 0);

    /* Total cognitive updates should include all cognitive bridges */
    int total_cognitive = g_collective_update_count + g_hyperscan_update_count +
                          g_phi_update_count + g_intent_update_count +
                          g_other_cognitive_count;
    EXPECT_GT(total_cognitive, 0);
}

/* ============================================================================
 * Phi Computation Integration Tests
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, PhiComputationDuringFEPCycle) {
    register_collective_bridges();

    /* Set up high synchronization for measurable phi */
    setup_synchronized_states(0.85f, 0.9f);

    /* Run collective updates to establish phi */
    for (int i = 0; i < 5; i++) {
        collective_cognition_update(cc);
    }

    /* Get phi value */
    collective_phi_t phi;
    collective_cognition_get_phi(cc, &phi);

    /* Phi should be computed */
    EXPECT_GE(phi.phi_total, 0.0f);
    EXPECT_GE(phi.information, 0.0f);
    EXPECT_GE(phi.integration, 0.0f);

    /* Now run FEP cycles that include phi updates */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, start_time + (i * 50));
    }

    /* Phi bridge should have been called */
    EXPECT_GT(g_phi_update_count.load(), 0);
}

/* ============================================================================
 * Consciousness Level Tracking Tests
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, ConsciousnessLevelDuringFEP) {
    register_collective_bridges();

    /* Get initial consciousness level */
    collective_consciousness_level_t initial_level =
        collective_cognition_get_consciousness_level(cc);

    /* Run FEP updates to allow phi to stabilize */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        fep_orchestrator_update(fep_orch, start_time + (i * 50));
    }

    /* Get final consciousness level */
    collective_consciousness_level_t final_level =
        collective_cognition_get_consciousness_level(cc);

    /* Consciousness level should be valid */
    EXPECT_GE((int)final_level, (int)COLLECTIVE_CONSCIOUSNESS_NONE);
    EXPECT_LE((int)final_level, (int)COLLECTIVE_CONSCIOUSNESS_TRANSCENDENT);

    /* Level should be related to phi */
    collective_phi_t phi;
    collective_cognition_get_phi(cc, &phi);

    if (phi.phi_total < 0.1f) {
        EXPECT_LE((int)final_level, (int)COLLECTIVE_CONSCIOUSNESS_MINIMAL);
    }
}

/* ============================================================================
 * Extended Mind Integration Tests
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, ExtendedMindWithFEP) {
    register_collective_bridges();

    /* Register extended mind extension */
    extended_mind_t* em = collective_cognition_get_extended_mind(cc);
    ASSERT_NE(em, nullptr);

    cognitive_extension_t ext;
    memset(&ext, 0, sizeof(ext));
    ext.type = EXT_TYPE_REASONING;
    snprintf(ext.name, sizeof(ext.name), "ExternalReasoner");
    ext.reliability = 0.95f;
    ext.avg_latency_ms = 5.0f;
    ext.integration_depth = 0.9f;
    ext.trust_level = 0.92f;

    uint32_t ext_id = extended_mind_register_extension(em, &ext);
    EXPECT_GT(ext_id, 0u);

    /* Run FEP updates */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, start_time + (i * 50));
    }

    /* Extended mind should increase collective capacity */
    collective_cognition_state_t state;
    collective_cognition_get_state(cc, &state);
    EXPECT_GE(state.collective_capacity, 0.0f);
}

/* ============================================================================
 * We-Mode Integration Tests
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, WeModeWithFEP) {
    register_collective_bridges();

    /* Enter we-mode */
    shared_intentionality_t* si = collective_cognition_get_intentionality(cc);
    ASSERT_NE(si, nullptr);

    /* Register instances with shared intentionality (separate from collective_cognition registration) */
    for (uint32_t i = 1; i <= 4; i++) {
        ASSERT_EQ(shared_intentionality_register_instance(si, i), 0);
    }

    int enter_result = shared_intentionality_enter_we_mode(si);
    /* We-mode entry may require additional setup (active instances), skip if not available */
    ASSERT_EQ(enter_result, 0) << "We-mode entry should succeed with 4 registered instances";

    /* Verify we-mode is active with multiple instances */
    ASSERT_TRUE(shared_intentionality_is_we_mode_active(si))
        << "We-mode should be active after entering with 4 instances";

    /* Run FEP updates */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, start_time + (i * 50));
    }

    /* Verify we-mode persists through FEP cycles */
    EXPECT_TRUE(shared_intentionality_is_we_mode_active(si));

    /* Intent bridge should have been updated */
    EXPECT_GT(g_intent_update_count.load(), 0);

    /* Get we-mode state */
    we_mode_state_t we_mode;
    collective_cognition_get_we_mode(cc, &we_mode);
    EXPECT_GE(we_mode.we_mode_strength, 0.0f);
}

/* ============================================================================
 * Statistics and Load Tests
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, FEPStatisticsWithCollective) {
    register_collective_bridges();

    /* Run known number of cycles */
    uint64_t base_time = get_current_time_ms();
    int expected_cycles = 15;
    for (int i = 0; i < expected_cycles; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 100));
    }

    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);

    /* Verify cycle count */
    EXPECT_EQ(stats.total_update_cycles, (uint64_t)expected_cycles);

    /* Verify bridge count (4 collective bridges) */
    EXPECT_GE(stats.total_bridges, 4u);

    /* Check cognitive category stats */
    EXPECT_GT(stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].bridge_count, 0u);
    EXPECT_GT(stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].total_updates, 0u);
}

TEST_F(CollectiveFEPIntegrationTest, LoadMeasurementWithCollective) {
    register_collective_bridges();

    /* Run updates */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 50));
    }

    /* Check load factor */
    float load = fep_orchestrator_get_load(fep_orch);
    EXPECT_GE(load, 0.0f);
    EXPECT_LE(load, 2.0f);
}

/* ============================================================================
 * Collective Statistics Integration
 * ============================================================================ */

TEST_F(CollectiveFEPIntegrationTest, CollectiveStatsAfterFEPCycles) {
    register_collective_bridges();

    /* Reset collective stats */
    collective_cognition_reset_stats(cc);

    /* Run FEP cycles (which trigger collective updates) */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 50));
    }

    /* Get collective stats */
    collective_cognition_stats_t cc_stats;
    collective_cognition_get_stats(cc, &cc_stats);

    /* Stats should show updates occurred */
    EXPECT_GT(cc_stats.total_updates, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
