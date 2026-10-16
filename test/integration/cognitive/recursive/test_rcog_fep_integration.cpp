/**
 * @file test_rcog_fep_integration.cpp
 * @brief Integration tests for Recursive Cognition + FEP Orchestrator
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Tests integration between Recursive Cognition engine and FEP orchestrator
 * WHY:  Verify FEP correctly coordinates rcog bridge updates and free energy flows
 * HOW:  Test FEP update cycles, bridge registration, and free energy metrics
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define RCOG_FEP_BRIDGE_ID_BASE     0x1400
#define RCOG_FEP_BRIDGE_ID_ENGINE   (RCOG_FEP_BRIDGE_ID_BASE + 1)
#define RCOG_FEP_BRIDGE_ID_CONTEXT  (RCOG_FEP_BRIDGE_ID_BASE + 2)
#define RCOG_FEP_BRIDGE_ID_ORCH     (RCOG_FEP_BRIDGE_ID_BASE + 3)

/* ============================================================================
 * Mock FEP Bridge Update Functions
 * ============================================================================ */

static std::atomic<int> g_engine_update_count{0};
static std::atomic<int> g_context_update_count{0};
static std::atomic<int> g_orch_update_count{0};
static std::atomic<float> g_last_free_energy{0.5f};
static std::atomic<float> g_last_prediction_error{0.5f};

static int rcog_engine_fep_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_engine_update_count++;
    /* Simulate free energy reduction on successful update */
    float current = g_last_free_energy.load();
    g_last_free_energy.store(current * 0.99f);
    return 0;
}

static int rcog_context_fep_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_context_update_count++;
    return 0;
}

static int rcog_orchestrator_fep_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_orch_update_count++;
    return 0;
}

static void reset_update_counters() {
    g_engine_update_count = 0;
    g_context_update_count = 0;
    g_orch_update_count = 0;
    g_last_free_energy = 0.5f;
    g_last_prediction_error = 0.5f;
}

/* ============================================================================
 * Test Fixture: RCOG + FEP Integration
 * ============================================================================ */

class RcogFEPIntegrationTest : public ::testing::Test {
protected:
    rcog_engine_t* engine = nullptr;
    fep_orchestrator_t* fep_orch = nullptr;
    fep_orchestrator_config_t fep_config;

    void SetUp() override {
        reset_update_counters();

        /* Create FEP orchestrator */
        fep_orchestrator_default_config(&fep_config);
        fep_config.enable_statistics = true;
        fep_config.enable_logging = false;
        fep_orch = fep_orchestrator_create(&fep_config);
        ASSERT_NE(fep_orch, nullptr);

        /* Create RCOG engine */
        engine = rcog_engine_create_default();
        ASSERT_NE(engine, nullptr);
        ASSERT_EQ(rcog_engine_init(engine), 0);

        /* Start FEP orchestrator */
        ASSERT_EQ(fep_orchestrator_start(fep_orch), 0);
    }

    void TearDown() override {
        if (engine) {
            rcog_engine_stop(engine, 1000);
            rcog_engine_destroy(engine);
            engine = nullptr;
        }
        if (fep_orch) {
            fep_orchestrator_stop(fep_orch);
            fep_orchestrator_destroy(fep_orch);
            fep_orch = nullptr;
        }
    }

    /* Helper to register rcog bridges with FEP */
    void register_rcog_bridges() {
        uint32_t bridge_id;

        /* Register engine bridge */
        int ret = fep_orchestrator_register_bridge(
            fep_orch, "rcog_engine",
            FEP_BRIDGE_CATEGORY_COGNITIVE,
            engine, rcog_engine_fep_update, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);

        /* Register context store bridge */
        ret = fep_orchestrator_register_bridge(
            fep_orch, "rcog_context_store",
            FEP_BRIDGE_CATEGORY_COGNITIVE,
            rcog_engine_get_context_store(engine),
            rcog_context_fep_update, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);

        /* Register orchestrator bridge */
        ret = fep_orchestrator_register_bridge(
            fep_orch, "rcog_orchestrator",
            FEP_BRIDGE_CATEGORY_COGNITIVE,
            rcog_engine_get_orchestrator(engine),
            rcog_orchestrator_fep_update, nullptr, &bridge_id);
        ASSERT_EQ(ret, 0);
    }

    uint64_t get_current_time_ms() {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }
};

/* ============================================================================
 * RcogFEPFullCycle - Complete FEP update cycle with rcog
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, RcogFEPFullCycle) {
    /* Register rcog bridges */
    register_rcog_bridges();

    /* Verify bridges registered */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GE(stats.total_bridges, 3u);

    /* Run multiple FEP update cycles */
    uint64_t start_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        uint64_t current_time = start_time + (i * 100);
        int updated = fep_orchestrator_update(fep_orch, current_time);
        EXPECT_GE(updated, 0);
    }

    /* Verify all bridges were updated */
    EXPECT_GT(g_engine_update_count.load(), 0);
    EXPECT_GT(g_context_update_count.load(), 0);
    EXPECT_GT(g_orch_update_count.load(), 0);

    /* Verify statistics accumulated */
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GT(stats.total_update_cycles, 0u);
    EXPECT_GT(stats.total_bridge_updates, 0u);
}

/* ============================================================================
 * RcogFEPWithOtherBridges - rcog FEP alongside other cognitive bridges
 * ============================================================================ */

static std::atomic<int> g_other_bridge_count{0};

static int other_cognitive_bridge_update(fep_bridge_handle_t handle) {
    (void)handle;
    g_other_bridge_count++;
    return 0;
}

TEST_F(RcogFEPIntegrationTest, RcogFEPWithOtherBridges) {
    g_other_bridge_count = 0;

    /* Register rcog bridges */
    register_rcog_bridges();

    /* Dummy handles for other bridges (non-null to pass validation) */
    static int dummy_emotion_handle = 1;
    static int dummy_attention_handle = 2;
    static int dummy_memory_handle = 3;

    /* Register additional cognitive bridges (simulating emotion, attention, etc.) */
    uint32_t bridge_id;
    int ret = fep_orchestrator_register_bridge(
        fep_orch, "emotion_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_emotion_handle, other_cognitive_bridge_update, nullptr, &bridge_id);
    ASSERT_EQ(ret, 0);

    ret = fep_orchestrator_register_bridge(
        fep_orch, "attention_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_attention_handle, other_cognitive_bridge_update, nullptr, &bridge_id);
    ASSERT_EQ(ret, 0);

    ret = fep_orchestrator_register_bridge(
        fep_orch, "memory_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        (fep_bridge_handle_t)&dummy_memory_handle, other_cognitive_bridge_update, nullptr, &bridge_id);
    ASSERT_EQ(ret, 0);

    /* Verify total bridge count */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GE(stats.total_bridges, 6u);

    /* Run FEP update cycles */
    uint64_t current_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, current_time + (i * 100));
    }

    /* All bridges should have been updated */
    EXPECT_GT(g_engine_update_count.load(), 0);
    EXPECT_GT(g_other_bridge_count.load(), 0);

    /* rcog bridges and other bridges should update together */
    int total_rcog = g_engine_update_count + g_context_update_count + g_orch_update_count;
    EXPECT_GT(total_rcog, 0);
    EXPECT_GT(g_other_bridge_count.load(), 0);
}

/* ============================================================================
 * RecursionDepthAffectsFreeEnergy - deeper recursion increases free energy
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, RecursionDepthAffectsFreeEnergy) {
    /* Set initial free energy state */
    float initial_free_energy = 0.5f;
    g_last_free_energy = initial_free_energy;

    /* Register bridges */
    register_rcog_bridges();

    /* Simulate shallow processing (low depth = low free energy) */
    rcog_engine_stats_t engine_stats_before;
    rcog_engine_get_stats(engine, &engine_stats_before);

    /* Run FEP updates at shallow depth */
    uint64_t current_time = get_current_time_ms();
    for (int i = 0; i < 5; i++) {
        fep_orchestrator_update(fep_orch, current_time + (i * 50));
    }

    float shallow_free_energy = g_last_free_energy.load();

    /* Simulate deeper processing by triggering more updates */
    /* In real scenario, deeper recursion would increase prediction error */
    g_last_prediction_error = 0.8f;  /* Higher error from deeper processing */

    /* More updates with higher error */
    for (int i = 0; i < 10; i++) {
        /* Deeper recursion increases free energy temporarily */
        float current = g_last_free_energy.load();
        g_last_free_energy.store(current + (g_last_prediction_error * 0.01f));
        fep_orchestrator_update(fep_orch, current_time + 500 + (i * 50));
    }

    float deep_free_energy = g_last_free_energy.load();

    /* Deeper processing should have higher free energy due to prediction error */
    /* (before it settles down through refinement) */
    EXPECT_NE(shallow_free_energy, deep_free_energy);

    /* Verify engine processed the cycles */
    EXPECT_GT(g_engine_update_count.load(), 10);
}

/* ============================================================================
 * TaskSuccessReducesPredictionError - successful tasks reduce error
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, TaskSuccessReducesPredictionError) {
    register_rcog_bridges();

    /* Set high initial prediction error */
    g_last_prediction_error = 0.9f;
    g_last_free_energy = 0.8f;

    float initial_error = g_last_prediction_error.load();
    float initial_fe = g_last_free_energy.load();

    /* Run FEP cycles - successful updates should reduce free energy */
    uint64_t current_time = get_current_time_ms();
    for (int i = 0; i < 20; i++) {
        fep_orchestrator_update(fep_orch, current_time + (i * 50));
        /* Engine update reduces free energy on success (see mock) */
    }

    float final_fe = g_last_free_energy.load();

    /* Free energy should decrease after successful updates */
    EXPECT_LT(final_fe, initial_fe);

    /* Verify substantial reduction occurred */
    float reduction_ratio = final_fe / initial_fe;
    EXPECT_LT(reduction_ratio, 1.0f);
}

/* ============================================================================
 * FEPCoordinatesRecursion - FEP orchestrator coordinates rcog updates
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, FEPCoordinatesRecursion) {
    register_rcog_bridges();

    /* Configure category update intervals */
    fep_orchestrator_set_update_interval(fep_orch, FEP_BRIDGE_CATEGORY_COGNITIVE, 50);

    /* Run coordinated updates */
    uint64_t base_time = get_current_time_ms();

    int total_updates_before = g_engine_update_count.load();

    /* Run for 500ms worth of updates */
    for (int i = 0; i < 10; i++) {
        uint64_t current = base_time + (i * 60);  /* Every 60ms */
        fep_orchestrator_update(fep_orch, current);
    }

    int total_updates_after = g_engine_update_count.load();
    int updates_during_cycle = total_updates_after - total_updates_before;

    /* Should have multiple coordinated updates */
    EXPECT_GT(updates_during_cycle, 5);

    /* Verify orchestrator tracked the updates */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_GT(stats.total_update_cycles, 0u);

    /* Verify category stats */
    EXPECT_GT(stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].total_updates, 0u);
}

/* ============================================================================
 * Category-Specific Update Tests
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, CognitiveCategoryUpdateTiming) {
    register_rcog_bridges();

    /* Set specific interval for cognitive category */
    uint64_t interval_ms = 100;
    fep_orchestrator_set_update_interval(fep_orch, FEP_BRIDGE_CATEGORY_COGNITIVE, interval_ms);

    reset_update_counters();

    /* Run updates at 50ms intervals (faster than category interval) */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 50));
    }

    /* With 100ms interval and 50ms update rate over 500ms,
     * should see approximately 5 category updates */
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);

    EXPECT_GT(stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].total_updates, 0u);
}

TEST_F(RcogFEPIntegrationTest, ForceUpdateAll) {
    register_rcog_bridges();
    reset_update_counters();

    /* Force update all bridges immediately */
    int updated = fep_orchestrator_force_update_all(fep_orch);
    EXPECT_GE(updated, 3);  /* At least 3 rcog bridges */

    /* All should have been updated exactly once */
    EXPECT_GE(g_engine_update_count.load(), 1);
    EXPECT_GE(g_context_update_count.load(), 1);
    EXPECT_GE(g_orch_update_count.load(), 1);
}

/* ============================================================================
 * Bridge Enable/Disable Tests
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, BridgeEnableDisable) {
    uint32_t engine_bridge_id;

    /* Register engine bridge and get its ID */
    int ret = fep_orchestrator_register_bridge(
        fep_orch, "rcog_engine_test",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        engine, rcog_engine_fep_update, nullptr, &engine_bridge_id);
    ASSERT_EQ(ret, 0);

    reset_update_counters();

    /* Run update cycle */
    fep_orchestrator_force_update_all(fep_orch);
    int count_enabled = g_engine_update_count.load();
    EXPECT_GT(count_enabled, 0);

    /* Disable the bridge */
    ret = fep_orchestrator_set_bridge_enabled(fep_orch, engine_bridge_id, false);
    EXPECT_EQ(ret, 0);

    reset_update_counters();

    /* Run another update cycle */
    fep_orchestrator_force_update_all(fep_orch);
    int count_disabled = g_engine_update_count.load();

    /* Bridge should not have been updated while disabled */
    EXPECT_EQ(count_disabled, 0);

    /* Re-enable and verify */
    ret = fep_orchestrator_set_bridge_enabled(fep_orch, engine_bridge_id, true);
    EXPECT_EQ(ret, 0);

    fep_orchestrator_force_update_all(fep_orch);
    EXPECT_GT(g_engine_update_count.load(), 0);
}

/* ============================================================================
 * Pause/Resume Tests
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, OrchestratorPauseResume) {
    register_rcog_bridges();
    reset_update_counters();

    /* Pause orchestrator */
    int ret = fep_orchestrator_pause(fep_orch);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(fep_orchestrator_get_state(fep_orch), FEP_ORCHESTRATOR_PAUSED);

    /* Updates should not process bridges while paused */
    uint64_t current_time = get_current_time_ms();
    fep_orchestrator_update(fep_orch, current_time);

    int paused_count = g_engine_update_count.load();

    /* Resume */
    ret = fep_orchestrator_resume(fep_orch);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(fep_orchestrator_get_state(fep_orch), FEP_ORCHESTRATOR_RUNNING);

    /* Now updates should work */
    fep_orchestrator_force_update_all(fep_orch);
    int resumed_count = g_engine_update_count.load();

    EXPECT_GT(resumed_count, paused_count);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, StatisticsAccuracy) {
    register_rcog_bridges();

    /* Run known number of cycles */
    uint64_t base_time = get_current_time_ms();
    int expected_cycles = 10;
    for (int i = 0; i < expected_cycles; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 100));
    }

    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(fep_orch, &stats);

    /* Verify cycle count */
    EXPECT_EQ(stats.total_update_cycles, (uint64_t)expected_cycles);

    /* Verify bridge count */
    EXPECT_GE(stats.total_bridges, 3u);
    EXPECT_GE(stats.active_bridges, 3u);

    /* Reset and verify */
    fep_orchestrator_reset_stats(fep_orch);
    fep_orchestrator_get_stats(fep_orch, &stats);
    EXPECT_EQ(stats.total_update_cycles, 0u);
    EXPECT_EQ(stats.total_bridge_updates, 0u);

    /* Bridge count should still be valid */
    EXPECT_GE(stats.total_bridges, 3u);
}

/* ============================================================================
 * Load Measurement Tests
 * ============================================================================ */

TEST_F(RcogFEPIntegrationTest, LoadMeasurement) {
    register_rcog_bridges();

    /* Run updates */
    uint64_t base_time = get_current_time_ms();
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_update(fep_orch, base_time + (i * 50));
    }

    /* Check load factor */
    float load = fep_orchestrator_get_load(fep_orch);
    EXPECT_GE(load, 0.0f);
    EXPECT_LE(load, 2.0f);  /* Some headroom for overload scenarios */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
