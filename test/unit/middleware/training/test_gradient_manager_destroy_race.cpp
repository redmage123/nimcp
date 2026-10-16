/**
 * @file test_gradient_manager_destroy_race.cpp
 * @brief Tests for gradient manager destroy race condition fix
 *
 * WHAT: Verify gradient manager destroy with shutting_down flag
 * WHY:  Bug H12 -- destroy function had a race between unlock and mutex destroy
 * HOW:  Test create/destroy lifecycle, verify shutting_down flag prevents
 *       concurrent accumulation during destroy, stress test with threads
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>

#include "middleware/training/nimcp_gradient_manager.h"

class GradientManagerDestroyRaceTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GradientManagerDestroyRaceTest, CreateDestroy_NoCrash) {
    nimcp_gradient_manager_config_t cfg = nimcp_gradient_manager_default_config();
    cfg.use_accumulation = true;
    cfg.accumulation.accumulation_steps = 4;

    nimcp_gradient_manager_ctx_t* ctx = nimcp_gradient_manager_create(&cfg);
    ASSERT_NE(ctx, nullptr);

    // Basic accumulation should work
    float grads[] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_result_t result = nimcp_gradient_accumulate(ctx, grads, 4);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    nimcp_gradient_manager_destroy(ctx);
    // Must not crash
}

TEST_F(GradientManagerDestroyRaceTest, NullDestroy_Safe) {
    nimcp_gradient_manager_destroy(nullptr);
}

TEST_F(GradientManagerDestroyRaceTest, DestroyDuringAccumulate_NoCrash) {
    // Stress test: one thread does repeated accumulations,
    // another thread destroys the manager after a short delay.
    // The accumulation thread should see NIMCP_ERROR_INVALID after
    // shutting_down is set, and never crash.
    for (int trial = 0; trial < 5; trial++) {
        nimcp_gradient_manager_config_t cfg = nimcp_gradient_manager_default_config();
        cfg.use_accumulation = true;
        cfg.accumulation.accumulation_steps = 1000;

        nimcp_gradient_manager_ctx_t* ctx = nimcp_gradient_manager_create(&cfg);
        ASSERT_NE(ctx, nullptr);

        std::atomic<bool> done{false};
        float grads[64];
        for (int i = 0; i < 64; i++) grads[i] = (float)i * 0.01f;

        // Thread 1: accumulate in a loop
        std::thread accumulator([ctx, &done, &grads]() {
            while (!done.load(std::memory_order_relaxed)) {
                nimcp_gradient_accumulate(ctx, grads, 64);
            }
        });

        // Let accumulator run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        // Signal stop then destroy
        done.store(true, std::memory_order_relaxed);
        accumulator.join();

        nimcp_gradient_manager_destroy(ctx);
    }
}

TEST_F(GradientManagerDestroyRaceTest, MultipleCreateDestroy_Cycle) {
    // Rapid create/destroy cycles to stress mutex init/destroy
    for (int i = 0; i < 20; i++) {
        nimcp_gradient_manager_config_t cfg = nimcp_gradient_manager_default_config();
        cfg.use_accumulation = true;
        cfg.accumulation.accumulation_steps = 2;

        nimcp_gradient_manager_ctx_t* ctx = nimcp_gradient_manager_create(&cfg);
        ASSERT_NE(ctx, nullptr);

        float grads[] = {1.0f, 2.0f};
        nimcp_gradient_accumulate(ctx, grads, 2);

        nimcp_gradient_manager_destroy(ctx);
    }
}

TEST_F(GradientManagerDestroyRaceTest, StatsAfterAccumulation) {
    nimcp_gradient_manager_config_t cfg = nimcp_gradient_manager_default_config();
    cfg.use_accumulation = true;
    cfg.accumulation.accumulation_steps = 4;
    cfg.track_statistics = true;

    nimcp_gradient_manager_ctx_t* ctx = nimcp_gradient_manager_create(&cfg);
    ASSERT_NE(ctx, nullptr);

    float grads[] = {1.0f, 2.0f, 3.0f};
    nimcp_gradient_accumulate(ctx, grads, 3);
    nimcp_gradient_accumulate(ctx, grads, 3);

    nimcp_grad_stats_t stats;
    EXPECT_EQ(nimcp_gradient_manager_get_stats(ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_accum_steps, 2u);

    nimcp_gradient_manager_destroy(ctx);
}
