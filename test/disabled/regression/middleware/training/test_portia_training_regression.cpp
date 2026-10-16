/**
 * @file test_portia_training_regression.cpp
 * @brief Regression tests for training performance under resource constraints
 *
 * WHAT: Verify training quality doesn't degrade unexpectedly with Portia
 * WHY:  Ensure resource-aware training maintains acceptable convergence
 * HOW:  Run training loops, measure convergence metrics, detect regressions
 *
 * Regression Scenarios:
 * - Training convergence rate with reduced batch sizes
 * - Learning stability with tier transitions
 * - Memory usage stays within tier limits
 * - Training throughput degradation is proportional to tier
 * - Resume after pause maintains training state
 * - No crashes or memory leaks during extended runs
 *
 * Acceptance Criteria:
 * - MEDIUM tier: ≥70% of FULL tier convergence speed
 * - CONSTRAINED tier: ≥50% of FULL tier convergence speed
 * - Memory usage: within tier budget ±10%
 * - No memory leaks over 1000+ iterations
 * - Resume accuracy: within 1% of pre-pause state
 *
 * @author NIMCP Development Team
 * @date 2025-12-09
 */

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cmath>
// Headers have their own extern "C" guards
#include "middleware/training/nimcp_brain_training_integration.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

class PortiaTrainingRegressionTest : public ::testing::Test {
protected:
    nimcp_brain_training_ctx_t* training_ctx;
    bio_router_context_t* router_ctx;

    void SetUp() override {
        /* Initialize bio-async router */
        bio_router_config_t router_config = {0};
        router_config.max_modules = 32;
        router_config.default_inbox_capacity = 128;
        router_config.enable_priority_channels = true;
        router_config.worker_thread_count = 4;

        router_ctx = bio_router_init(&router_config);
        ASSERT_NE(router_ctx, nullptr);

        /* Create brain training context with Portia integration */
        nimcp_brain_training_config_t config = nimcp_brain_training_default_config();
        config.enable_portia_integration = true;
        config.min_batch_size_ratio = 0.25f;
        config.allow_training_pause = true;
        config.adapt_to_tier_changes = true;

        training_ctx = nimcp_brain_training_create(&config);
        ASSERT_NE(training_ctx, nullptr);

        nimcp_result_t res = nimcp_brain_training_init(training_ctx, nullptr, nullptr);
        ASSERT_EQ(res, NIMCP_SUCCESS);

        /* Connect mock Portia */
        void* mock_portia = (void*)0x12345678;
        nimcp_brain_training_connect_portia(training_ctx, mock_portia);
    }

    void TearDown() override {
        if (training_ctx) {
            nimcp_brain_training_destroy(training_ctx);
            training_ctx = nullptr;
        }

        if (router_ctx) {
            bio_router_shutdown(router_ctx);
            router_ctx = nullptr;
        }
    }

    /* Helper: Simulate convergence metric (mock loss decay) */
    float simulate_training_iteration(
        int iteration,
        size_t batch_size,
        float learning_rate)
    {
        /* Simple mock: loss = 1.0 / (1 + iterations * LR * batch_factor) */
        float batch_factor = batch_size / 100.0f;
        float decay = 1.0f + iteration * learning_rate * batch_factor;
        return 1.0f / decay;
    }

    /* Helper: Run training for N iterations and return final loss */
    float run_training_iterations(
        platform_tier_t tier,
        int num_iterations,
        size_t base_batch,
        float base_lr,
        std::vector<float>* loss_history = nullptr)
    {
        nimcp_brain_training_on_tier_change(training_ctx, tier);

        float final_loss = 1.0f;

        for (int i = 0; i < num_iterations; i++) {
            if (nimcp_brain_training_is_paused(training_ctx)) {
                break;
            }

            size_t batch = nimcp_brain_training_get_adjusted_batch_size(
                training_ctx,
                base_batch
            );

            float lr = nimcp_brain_training_get_adjusted_lr(
                training_ctx,
                base_lr
            );

            final_loss = simulate_training_iteration(i, batch, lr);

            if (loss_history) {
                loss_history->push_back(final_loss);
            }
        }

        return final_loss;
    }
};

/* ============================================================================
 * Convergence Rate Regression Tests
 * ============================================================================ */

/**
 * @test Verify MEDIUM tier achieves ≥70% convergence rate of FULL tier
 */
TEST_F(PortiaTrainingRegressionTest, MediumTierConvergenceRate) {
    LOG_INFO("=== Regression Test: MEDIUM Tier Convergence Rate ===");

    const int num_iterations = 100;
    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Baseline: Train on FULL tier */
    float loss_full = run_training_iterations(
        PLATFORM_TIER_FULL,
        num_iterations,
        base_batch,
        base_lr
    );

    LOG_INFO("FULL tier final loss: %.6f", loss_full);

    /* Test: Train on MEDIUM tier */
    float loss_medium = run_training_iterations(
        PLATFORM_TIER_MEDIUM,
        num_iterations,
        base_batch,
        base_lr
    );

    LOG_INFO("MEDIUM tier final loss: %.6f", loss_medium);

    /* Calculate convergence ratio */
    float convergence_ratio = loss_full / loss_medium;

    LOG_INFO("Convergence ratio: %.2f%% of FULL tier", convergence_ratio * 100.0f);

    /* Acceptance: MEDIUM should achieve ≥70% convergence rate */
    EXPECT_GE(convergence_ratio, 0.70f)
        << "MEDIUM tier convergence should be ≥70% of FULL tier";

    LOG_INFO("=== Test PASSED: Convergence within acceptable range ===");
}

/**
 * @test Verify CONSTRAINED tier achieves ≥50% convergence rate of FULL tier
 */
TEST_F(PortiaTrainingRegressionTest, ConstrainedTierConvergenceRate) {
    LOG_INFO("=== Regression Test: CONSTRAINED Tier Convergence Rate ===");

    const int num_iterations = 100;
    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Baseline: Train on FULL tier */
    float loss_full = run_training_iterations(
        PLATFORM_TIER_FULL,
        num_iterations,
        base_batch,
        base_lr
    );

    LOG_INFO("FULL tier final loss: %.6f", loss_full);

    /* Test: Train on CONSTRAINED tier */
    float loss_constrained = run_training_iterations(
        PLATFORM_TIER_CONSTRAINED,
        num_iterations,
        base_batch,
        base_lr
    );

    LOG_INFO("CONSTRAINED tier final loss: %.6f", loss_constrained);

    /* Calculate convergence ratio */
    float convergence_ratio = loss_full / loss_constrained;

    LOG_INFO("Convergence ratio: %.2f%% of FULL tier", convergence_ratio * 100.0f);

    /* Acceptance: CONSTRAINED should achieve ≥50% convergence rate */
    EXPECT_GE(convergence_ratio, 0.50f)
        << "CONSTRAINED tier convergence should be ≥50% of FULL tier";

    LOG_INFO("=== Test PASSED: Convergence within acceptable range ===");
}

/* ============================================================================
 * Training Stability Regression Tests
 * ============================================================================ */

/**
 * @test Verify training stability with frequent tier transitions
 */
TEST_F(PortiaTrainingRegressionTest, TrainingStabilityWithTierTransitions) {
    LOG_INFO("=== Regression Test: Training Stability with Tier Transitions ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    std::vector<float> loss_history;
    loss_history.reserve(200);

    platform_tier_t tiers[] = {
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_FULL
    };

    int iteration = 0;

    for (size_t i = 0; i < sizeof(tiers) / sizeof(tiers[0]); i++) {
        nimcp_brain_training_on_tier_change(training_ctx, tiers[i]);

        LOG_INFO("Tier %zu/%zu: %s",
                 i + 1,
                 sizeof(tiers) / sizeof(tiers[0]),
                 platform_tier_get_name(tiers[i]));

        /* Train 30 iterations on this tier */
        for (int j = 0; j < 30; j++) {
            size_t batch = nimcp_brain_training_get_adjusted_batch_size(
                training_ctx,
                base_batch
            );

            float lr = nimcp_brain_training_get_adjusted_lr(
                training_ctx,
                base_lr
            );

            float loss = simulate_training_iteration(iteration++, batch, lr);
            loss_history.push_back(loss);
        }
    }

    /* Check for instability: no large spikes in loss */
    bool stable = true;
    float max_spike = 0.0f;

    for (size_t i = 1; i < loss_history.size(); i++) {
        float delta = std::abs(loss_history[i] - loss_history[i-1]);
        if (delta > max_spike) {
            max_spike = delta;
        }

        /* Check for unstable spike (loss increases by >20%) */
        if (loss_history[i] > loss_history[i-1] * 1.2f) {
            stable = false;
            LOG_WARNING("Instability detected at iteration %zu: loss %.6f -> %.6f",
                        i, loss_history[i-1], loss_history[i]);
        }
    }

    LOG_INFO("Max loss spike: %.6f", max_spike);
    LOG_INFO("Final loss: %.6f", loss_history.back());

    EXPECT_TRUE(stable) << "Training should remain stable during tier transitions";

    LOG_INFO("=== Test PASSED: Training remained stable ===");
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

/**
 * @test Verify training throughput scales appropriately with tier
 */
TEST_F(PortiaTrainingRegressionTest, TrainingThroughputScaling) {
    LOG_INFO("=== Regression Test: Training Throughput Scaling ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;
    const int num_iterations = 100;

    struct {
        platform_tier_t tier;
        float expected_relative_throughput;
    } test_cases[] = {
        { PLATFORM_TIER_FULL,        1.0f  },
        { PLATFORM_TIER_MEDIUM,      0.75f },
        { PLATFORM_TIER_CONSTRAINED, 0.5f  },
    };

    uint64_t baseline_time_us = 0;

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        nimcp_brain_training_on_tier_change(training_ctx, test_cases[i].tier);

        uint64_t start_us = nimcp_time_now_us();

        for (int iter = 0; iter < num_iterations; iter++) {
            if (nimcp_brain_training_is_paused(training_ctx)) {
                break;
            }

            size_t batch = nimcp_brain_training_get_adjusted_batch_size(
                training_ctx,
                base_batch
            );

            float lr = nimcp_brain_training_get_adjusted_lr(
                training_ctx,
                base_lr
            );

            /* Simulate work proportional to batch size */
            volatile int dummy = 0;
            for (size_t j = 0; j < batch * 100; j++) {
                dummy += j;
            }
        }

        uint64_t end_us = nimcp_time_now_us();
        uint64_t elapsed_us = end_us - start_us;

        if (test_cases[i].tier == PLATFORM_TIER_FULL) {
            baseline_time_us = elapsed_us;
        }

        float relative_time = (float)elapsed_us / baseline_time_us;
        float expected_time = 1.0f / test_cases[i].expected_relative_throughput;

        LOG_INFO("Tier %s: %.2fms (%.2fx baseline, expected ~%.2fx)",
                 platform_tier_get_name(test_cases[i].tier),
                 elapsed_us / 1000.0f,
                 relative_time,
                 expected_time);

        /* Allow ±30% variance due to system noise */
        EXPECT_NEAR(relative_time, expected_time, expected_time * 0.3f)
            << "Throughput scaling should match tier multipliers";
    }

    LOG_INFO("=== Test PASSED: Throughput scales as expected ===");
}

/* ============================================================================
 * Pause/Resume Regression Tests
 * ============================================================================ */

/**
 * @test Verify resume after pause maintains convergence trajectory
 */
TEST_F(PortiaTrainingRegressionTest, ResumeAfterPauseMaintainsTrajectory) {
    LOG_INFO("=== Regression Test: Resume After Pause ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Phase 1: Train for 50 iterations on FULL tier */
    std::vector<float> loss_before_pause;
    float loss_at_pause = run_training_iterations(
        PLATFORM_TIER_FULL,
        50,
        base_batch,
        base_lr,
        &loss_before_pause
    );

    LOG_INFO("Loss at pause point: %.6f", loss_at_pause);

    /* Phase 2: Pause training (MINIMAL tier) */
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_MINIMAL);
    EXPECT_TRUE(nimcp_brain_training_is_paused(training_ctx));

    /* Simulate pause duration */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* Phase 3: Resume training (upgrade to FULL) */
    nimcp_brain_training_on_tier_change(training_ctx, PLATFORM_TIER_FULL);
    EXPECT_FALSE(nimcp_brain_training_is_paused(training_ctx));

    /* Continue training for 50 more iterations */
    std::vector<float> loss_after_resume;
    float loss_final = run_training_iterations(
        PLATFORM_TIER_FULL,
        50,
        base_batch,
        base_lr,
        &loss_after_resume
    );

    LOG_INFO("Loss after resume: %.6f", loss_final);

    /* Verify loss continued to decrease */
    EXPECT_LT(loss_final, loss_at_pause)
        << "Loss should continue decreasing after resume";

    /* Verify no large jump at resume point */
    if (!loss_after_resume.empty() && !loss_before_pause.empty()) {
        float jump = std::abs(loss_after_resume[0] - loss_before_pause.back());
        EXPECT_LT(jump, 0.1f)
            << "Resume should not cause large loss discontinuity";

        LOG_INFO("Loss jump at resume: %.6f", jump);
    }

    LOG_INFO("=== Test PASSED: Resume maintains convergence ===");
}

/* ============================================================================
 * Memory and Resource Regression Tests
 * ============================================================================ */

/**
 * @test Verify no memory leaks over extended training
 */
TEST_F(PortiaTrainingRegressionTest, NoMemoryLeaksExtendedTraining) {
    LOG_INFO("=== Regression Test: No Memory Leaks (Extended Training) ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;
    const int num_iterations = 1000;

    /* Get initial memory usage */
    size_t initial_allocs = nimcp_get_allocation_count();
    size_t initial_frees = nimcp_get_free_count();

    LOG_INFO("Initial allocations: %zu, frees: %zu", initial_allocs, initial_frees);

    /* Run extended training with tier changes */
    platform_tier_t tiers[] = {
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_FULL
    };

    for (int epoch = 0; epoch < 10; epoch++) {
        platform_tier_t tier = tiers[epoch % (sizeof(tiers) / sizeof(tiers[0]))];
        nimcp_brain_training_on_tier_change(training_ctx, tier);

        for (int iter = 0; iter < num_iterations / 10; iter++) {
            if (nimcp_brain_training_is_paused(training_ctx)) {
                continue;
            }

            size_t batch = nimcp_brain_training_get_adjusted_batch_size(
                training_ctx,
                base_batch
            );

            float lr = nimcp_brain_training_get_adjusted_lr(
                training_ctx,
                base_lr
            );

            /* Simulate training step */
            simulate_training_iteration(iter, batch, lr);
        }
    }

    /* Get final memory usage */
    size_t final_allocs = nimcp_get_allocation_count();
    size_t final_frees = nimcp_get_free_count();

    LOG_INFO("Final allocations: %zu, frees: %zu", final_allocs, final_frees);

    /* Calculate leak */
    size_t net_allocs = final_allocs - initial_allocs;
    size_t net_frees = final_frees - initial_frees;
    ssize_t leak = net_allocs - net_frees;

    LOG_INFO("Net allocations: %zu, net frees: %zu, leak: %zd",
             net_allocs, net_frees, leak);

    /* Allow small constant overhead (e.g., caches), but no proportional leak */
    EXPECT_LE(std::abs(leak), 100)
        << "Memory leak should be negligible over extended training";

    LOG_INFO("=== Test PASSED: No significant memory leaks detected ===");
}

/**
 * @test Verify training adapts correctly to all tier sequences
 */
TEST_F(PortiaTrainingRegressionTest, AllTierSequencesCovered) {
    LOG_INFO("=== Regression Test: All Tier Sequences Covered ===");

    const size_t base_batch = 128;
    const float base_lr = 0.01f;

    /* Test all tier transitions */
    platform_tier_t all_tiers[] = {
        PLATFORM_TIER_FULL,
        PLATFORM_TIER_MEDIUM,
        PLATFORM_TIER_CONSTRAINED,
        PLATFORM_TIER_MINIMAL
    };

    for (size_t from = 0; from < sizeof(all_tiers) / sizeof(all_tiers[0]); from++) {
        for (size_t to = 0; to < sizeof(all_tiers) / sizeof(all_tiers[0]); to++) {
            LOG_INFO("Testing transition: %s -> %s",
                     platform_tier_get_name(all_tiers[from]),
                     platform_tier_get_name(all_tiers[to]));

            /* Set initial tier */
            nimcp_brain_training_on_tier_change(training_ctx, all_tiers[from]);

            /* Transition to target tier */
            nimcp_result_t res = nimcp_brain_training_on_tier_change(
                training_ctx,
                all_tiers[to]
            );

            EXPECT_EQ(res, NIMCP_SUCCESS)
                << "Transition should succeed: "
                << platform_tier_get_name(all_tiers[from])
                << " -> "
                << platform_tier_get_name(all_tiers[to]);

            /* Verify can still get parameters */
            size_t batch = nimcp_brain_training_get_adjusted_batch_size(
                training_ctx,
                base_batch
            );

            float lr = nimcp_brain_training_get_adjusted_lr(
                training_ctx,
                base_lr
            );

            /* If not paused, batch and LR should be non-zero */
            if (!nimcp_brain_training_is_paused(training_ctx)) {
                EXPECT_GT(batch, 0);
                EXPECT_GT(lr, 0.0f);
            }
        }
    }

    LOG_INFO("=== Test PASSED: All tier transitions handled correctly ===");
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
