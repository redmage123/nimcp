/**
 * @file test_tier3_fep_bridges_integration.cpp
 * @brief Integration tests for Tier 3 FEP bridges (FEP bridges for Hub bridges)
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Integration tests for Tier 3 FEP bridges working together
 * WHY:  Verify FEP bridges interact correctly with Hub bridges and FEP orchestrator
 * HOW:  Test multi-bridge scenarios, cross-module free energy flow
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

#include "cognitive/integration/nimcp_imagination_reasoning_fep_bridge.h"
#include "cognitive/integration/nimcp_game_theory_executive_fep_bridge.h"
#include "cognitive/integration/nimcp_mirror_empathy_fep_bridge.h"
#include "cognitive/integration/nimcp_salience_attention_fep_bridge.h"
#include "cognitive/integration/nimcp_predictive_attention_fep_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class Tier3FEPBridgesIntegrationTest : public ::testing::Test {
protected:
    imag_reason_fep_bridge_t* ir_bridge = nullptr;
    gt_exec_fep_bridge_t* gt_bridge = nullptr;
    me_fep_bridge_t* me_bridge = nullptr;
    sa_fep_bridge_t* sa_bridge = nullptr;
    pa_fep_bridge_t* pa_bridge = nullptr;

    void SetUp() override {
        ir_bridge = imag_reason_fep_bridge_create(nullptr);
        gt_bridge = gt_exec_fep_bridge_create(nullptr);
        me_bridge = me_fep_bridge_create(nullptr);
        sa_bridge = sa_fep_bridge_create(nullptr);
        pa_bridge = pa_fep_bridge_create(nullptr);
    }

    void TearDown() override {
        if (ir_bridge) imag_reason_fep_bridge_destroy(ir_bridge);
        if (gt_bridge) gt_exec_fep_bridge_destroy(gt_bridge);
        if (me_bridge) me_fep_bridge_destroy(me_bridge);
        if (sa_bridge) sa_fep_bridge_destroy(sa_bridge);
        if (pa_bridge) pa_fep_bridge_destroy(pa_bridge);
    }
};

//=============================================================================
// Integration Tests
//=============================================================================

/**
 * Test: AllBridgesCreateWithDefaultConfig
 * All bridges should be created with default configuration
 */
TEST_F(Tier3FEPBridgesIntegrationTest, AllBridgesCreateWithDefaultConfig) {
    EXPECT_NE(ir_bridge, nullptr);
    EXPECT_NE(gt_bridge, nullptr);
    EXPECT_NE(me_bridge, nullptr);
    EXPECT_NE(sa_bridge, nullptr);
    EXPECT_NE(pa_bridge, nullptr);
}

/**
 * Test: CoordinatedFreeEnergyUpdate
 * All bridges should update free energy in a coordinated manner
 */
TEST_F(Tier3FEPBridgesIntegrationTest, CoordinatedFreeEnergyUpdate) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    // Set metrics that represent "processing load"
    imag_reason_fep_bridge_update_scenario_quality(ir_bridge, 0.5f);
    gt_exec_fep_bridge_update_decision_quality(gt_bridge, 0.5f);
    me_fep_bridge_update_mirroring_error(me_bridge, 0.5f);
    sa_fep_bridge_update_salience_error(sa_bridge, 0.5f);
    pa_fep_bridge_update_prediction_accuracy(pa_bridge, 0.5f);

    // Force updates
    imag_reason_fep_bridge_force_update(ir_bridge);
    gt_exec_fep_bridge_force_update(gt_bridge);
    me_fep_bridge_force_update(me_bridge);
    sa_fep_bridge_force_update(sa_bridge);
    pa_fep_bridge_force_update(pa_bridge);

    // Calculate aggregate free energy
    float total_fe = imag_reason_fep_bridge_get_free_energy(ir_bridge) +
                     gt_exec_fep_bridge_get_free_energy(gt_bridge) +
                     me_fep_bridge_get_free_energy(me_bridge) +
                     sa_fep_bridge_get_free_energy(sa_bridge) +
                     pa_fep_bridge_get_free_energy(pa_bridge);

    EXPECT_GT(total_fe, 0.0f) << "Total FE should be positive";
    EXPECT_LT(total_fe, 10.0f) << "Total FE should be reasonable";
}

/**
 * Test: CrossModuleFreeEnergyPropagation
 * Changes in one bridge should not directly affect others
 */
TEST_F(Tier3FEPBridgesIntegrationTest, CrossModuleFreeEnergyPropagation) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);

    // Get initial FE values
    float ir_initial = imag_reason_fep_bridge_get_free_energy(ir_bridge);
    float gt_initial = gt_exec_fep_bridge_get_free_energy(gt_bridge);

    // Update only imagination-reasoning
    imag_reason_fep_bridge_update_scenario_quality(ir_bridge, 0.1f);
    imag_reason_fep_bridge_force_update(ir_bridge);

    float ir_after = imag_reason_fep_bridge_get_free_energy(ir_bridge);
    float gt_after = gt_exec_fep_bridge_get_free_energy(gt_bridge);

    // IR should change, GT should remain same
    EXPECT_NE(ir_after, ir_initial) << "IR FE should change";
    EXPECT_EQ(gt_after, gt_initial) << "GT FE should not change";
}

/**
 * Test: SequentialMultiBridgeUpdate
 * Sequential updates across bridges should work correctly
 */
TEST_F(Tier3FEPBridgesIntegrationTest, SequentialMultiBridgeUpdate) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    std::vector<float> fe_history;

    // Simulate processing pipeline
    for (int i = 0; i < 20; i++) {
        float quality = 0.3f + 0.05f * i;  // Gradually improving

        imag_reason_fep_bridge_update_scenario_quality(ir_bridge, quality);
        imag_reason_fep_bridge_force_update(ir_bridge);

        gt_exec_fep_bridge_update_decision_quality(gt_bridge, quality);
        gt_exec_fep_bridge_force_update(gt_bridge);

        me_fep_bridge_update_empathy_error(me_bridge, 1.0f - quality);
        me_fep_bridge_force_update(me_bridge);

        sa_fep_bridge_update_attention_efficiency(sa_bridge, quality);
        sa_fep_bridge_force_update(sa_bridge);

        pa_fep_bridge_update_attention_precision(pa_bridge, quality);
        pa_fep_bridge_force_update(pa_bridge);

        float total_fe = imag_reason_fep_bridge_get_free_energy(ir_bridge) +
                         gt_exec_fep_bridge_get_free_energy(gt_bridge) +
                         me_fep_bridge_get_free_energy(me_bridge) +
                         sa_fep_bridge_get_free_energy(sa_bridge) +
                         pa_fep_bridge_get_free_energy(pa_bridge);

        fe_history.push_back(total_fe);
    }

    // Total FE should generally decrease as quality improves
    float first_half_avg = 0.0f, second_half_avg = 0.0f;
    for (size_t i = 0; i < fe_history.size(); i++) {
        if (i < fe_history.size() / 2) {
            first_half_avg += fe_history[i];
        } else {
            second_half_avg += fe_history[i];
        }
    }
    first_half_avg /= (fe_history.size() / 2);
    second_half_avg /= (fe_history.size() / 2);

    // Second half should have lower or equal average FE
    // (allowing for some tolerance due to different metrics)
    EXPECT_LE(second_half_avg, first_half_avg + 0.5f)
        << "FE should generally decrease with improving quality";
}

/**
 * Test: ConcurrentBridgeAccess
 * Multiple threads can update different bridges concurrently
 */
TEST_F(Tier3FEPBridgesIntegrationTest, ConcurrentBridgeAccess) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    std::atomic<int> completed{0};
    constexpr int ITERATIONS = 50;

    auto update_ir = [this, &completed]() {
        for (int i = 0; i < ITERATIONS; i++) {
            imag_reason_fep_bridge_update_scenario_quality(ir_bridge, 0.5f);
            imag_reason_fep_bridge_force_update(ir_bridge);
        }
        completed++;
    };

    auto update_gt = [this, &completed]() {
        for (int i = 0; i < ITERATIONS; i++) {
            gt_exec_fep_bridge_update_decision_quality(gt_bridge, 0.5f);
            gt_exec_fep_bridge_force_update(gt_bridge);
        }
        completed++;
    };

    auto update_me = [this, &completed]() {
        for (int i = 0; i < ITERATIONS; i++) {
            me_fep_bridge_update_empathy_error(me_bridge, 0.5f);
            me_fep_bridge_force_update(me_bridge);
        }
        completed++;
    };

    auto update_sa = [this, &completed]() {
        for (int i = 0; i < ITERATIONS; i++) {
            sa_fep_bridge_update_salience_error(sa_bridge, 0.5f);
            sa_fep_bridge_force_update(sa_bridge);
        }
        completed++;
    };

    auto update_pa = [this, &completed]() {
        for (int i = 0; i < ITERATIONS; i++) {
            pa_fep_bridge_update_prediction_accuracy(pa_bridge, 0.5f);
            pa_fep_bridge_force_update(pa_bridge);
        }
        completed++;
    };

    std::thread t1(update_ir);
    std::thread t2(update_gt);
    std::thread t3(update_me);
    std::thread t4(update_sa);
    std::thread t5(update_pa);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

    EXPECT_EQ(completed.load(), 5) << "All threads should complete";

    // All bridges should still be in valid state
    EXPECT_GE(imag_reason_fep_bridge_get_free_energy(ir_bridge), 0.0f);
    EXPECT_GE(gt_exec_fep_bridge_get_free_energy(gt_bridge), 0.0f);
    EXPECT_GE(me_fep_bridge_get_free_energy(me_bridge), 0.0f);
    EXPECT_GE(sa_fep_bridge_get_free_energy(sa_bridge), 0.0f);
    EXPECT_GE(pa_fep_bridge_get_free_energy(pa_bridge), 0.0f);
}

/**
 * Test: ResetAllBridges
 * Resetting all bridges should restore initial state
 */
TEST_F(Tier3FEPBridgesIntegrationTest, ResetAllBridges) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    // Get initial FE values
    float ir_init = imag_reason_fep_bridge_get_free_energy(ir_bridge);
    float gt_init = gt_exec_fep_bridge_get_free_energy(gt_bridge);
    float me_init = me_fep_bridge_get_free_energy(me_bridge);
    float sa_init = sa_fep_bridge_get_free_energy(sa_bridge);
    float pa_init = pa_fep_bridge_get_free_energy(pa_bridge);

    // Perturb all bridges
    imag_reason_fep_bridge_update_scenario_quality(ir_bridge, 0.1f);
    gt_exec_fep_bridge_update_decision_quality(gt_bridge, 0.1f);
    me_fep_bridge_update_mirroring_error(me_bridge, 0.9f);
    sa_fep_bridge_update_salience_error(sa_bridge, 0.9f);
    pa_fep_bridge_update_prediction_accuracy(pa_bridge, 0.1f);

    imag_reason_fep_bridge_force_update(ir_bridge);
    gt_exec_fep_bridge_force_update(gt_bridge);
    me_fep_bridge_force_update(me_bridge);
    sa_fep_bridge_force_update(sa_bridge);
    pa_fep_bridge_force_update(pa_bridge);

    // Reset all
    imag_reason_fep_bridge_reset(ir_bridge);
    gt_exec_fep_bridge_reset(gt_bridge);
    me_fep_bridge_reset(me_bridge);
    sa_fep_bridge_reset(sa_bridge);
    pa_fep_bridge_reset(pa_bridge);

    // Verify restoration
    EXPECT_NEAR(imag_reason_fep_bridge_get_free_energy(ir_bridge), ir_init, 0.01f);
    EXPECT_NEAR(gt_exec_fep_bridge_get_free_energy(gt_bridge), gt_init, 0.01f);
    EXPECT_NEAR(me_fep_bridge_get_free_energy(me_bridge), me_init, 0.01f);
    EXPECT_NEAR(sa_fep_bridge_get_free_energy(sa_bridge), sa_init, 0.01f);
    EXPECT_NEAR(pa_fep_bridge_get_free_energy(pa_bridge), pa_init, 0.01f);
}

/**
 * Test: StatisticsAccumulationAcrossBridges
 * Statistics should accumulate correctly across all bridges
 */
TEST_F(Tier3FEPBridgesIntegrationTest, StatisticsAccumulationAcrossBridges) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    constexpr int UPDATES = 10;

    for (int i = 0; i < UPDATES; i++) {
        imag_reason_fep_bridge_force_update(ir_bridge);
        gt_exec_fep_bridge_force_update(gt_bridge);
        me_fep_bridge_force_update(me_bridge);
        sa_fep_bridge_force_update(sa_bridge);
        pa_fep_bridge_force_update(pa_bridge);
    }

    imag_reason_fep_stats_t ir_stats;
    gt_exec_fep_stats_t gt_stats;
    me_fep_stats_t me_stats;
    sa_fep_stats_t sa_stats;
    pa_fep_stats_t pa_stats;

    imag_reason_fep_bridge_get_stats(ir_bridge, &ir_stats);
    gt_exec_fep_bridge_get_stats(gt_bridge, &gt_stats);
    me_fep_bridge_get_stats(me_bridge, &me_stats);
    sa_fep_bridge_get_stats(sa_bridge, &sa_stats);
    pa_fep_bridge_get_stats(pa_bridge, &pa_stats);

    EXPECT_EQ(ir_stats.total_updates, (uint64_t)UPDATES);
    EXPECT_EQ(gt_stats.total_updates, (uint64_t)UPDATES);
    EXPECT_EQ(me_stats.total_updates, (uint64_t)UPDATES);
    EXPECT_EQ(sa_stats.total_updates, (uint64_t)UPDATES);
    EXPECT_EQ(pa_stats.total_updates, (uint64_t)UPDATES);

    uint64_t total_updates = ir_stats.total_updates + gt_stats.total_updates +
                             me_stats.total_updates + sa_stats.total_updates +
                             pa_stats.total_updates;
    EXPECT_EQ(total_updates, (uint64_t)(5 * UPDATES));
}

/**
 * Test: PartialBridgeFailure
 * System should continue with some bridges destroyed
 */
TEST_F(Tier3FEPBridgesIntegrationTest, PartialBridgeFailure) {
    ASSERT_NE(ir_bridge, nullptr);
    ASSERT_NE(gt_bridge, nullptr);
    ASSERT_NE(me_bridge, nullptr);
    ASSERT_NE(sa_bridge, nullptr);
    ASSERT_NE(pa_bridge, nullptr);

    // Destroy some bridges
    imag_reason_fep_bridge_destroy(ir_bridge);
    ir_bridge = nullptr;

    gt_exec_fep_bridge_destroy(gt_bridge);
    gt_bridge = nullptr;

    // Remaining bridges should still work
    me_fep_bridge_update_empathy_error(me_bridge, 0.5f);
    me_fep_bridge_force_update(me_bridge);

    sa_fep_bridge_update_salience_error(sa_bridge, 0.5f);
    sa_fep_bridge_force_update(sa_bridge);

    pa_fep_bridge_update_prediction_accuracy(pa_bridge, 0.5f);
    pa_fep_bridge_force_update(pa_bridge);

    EXPECT_GE(me_fep_bridge_get_free_energy(me_bridge), 0.0f);
    EXPECT_GE(sa_fep_bridge_get_free_energy(sa_bridge), 0.0f);
    EXPECT_GE(pa_fep_bridge_get_free_energy(pa_bridge), 0.0f);
}
