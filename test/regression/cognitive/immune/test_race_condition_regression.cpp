/**
 * @file test_race_condition_regression.cpp
 * @brief Regression tests for race conditions in immune system, sensory bridge,
 *        and SNN surrogate backward NULL deref fixes.
 *
 * WHAT: Verify fixes for C1/H2 (immune antigen race), C3 (sensory update race),
 *       and C2 (SNN surrogate NULL deref)
 * WHY:  Prevent regression of thread-safety and NULL pointer fixes
 * HOW:  GoogleTest with multi-threaded antigen access, sensory update, and
 *       NULL gradient scenarios
 *
 * @date 2026-03-05
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "perception/nimcp_omni_sensory_bridge.h"
#include "snn/nimcp_snn_training.h"
#include "snn/nimcp_snn_types.h"

/* ============================================================================
 * Bug C1/H2: Immune Antigen Race Condition Regression Tests
 * ============================================================================ */

class ImmuneAntigenRaceTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune = nullptr;
    brain_immune_config_t config;

    void SetUp() override {
        brain_immune_default_config(&config);
        config.max_antigens = 256;
        immune = brain_immune_create(&config);
        ASSERT_NE(immune, nullptr);
        brain_immune_start(immune);
    }

    void TearDown() override {
        if (immune) {
            brain_immune_stop(immune);
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }
};

/**
 * @brief Verify brain_immune_present_antigen is thread-safe with concurrent calls
 *
 * WHAT: Multiple threads present antigens simultaneously
 * WHY:  Regression for C1 - race on antigen list modification
 * HOW:  4 threads each present 10 antigens; verify no crash and count matches
 */
TEST_F(ImmuneAntigenRaceTest, ConcurrentAntigenPresentation_NoCrash) {
    const int NUM_THREADS = 4;
    const int ANTIGENS_PER_THREAD = 10;
    std::atomic<int> success_count{0};

    auto presenter = [&](int thread_id) {
        for (int i = 0; i < ANTIGENS_PER_THREAD; i++) {
            uint8_t epitope[8];
            memset(epitope, (uint8_t)(thread_id * 16 + i), sizeof(epitope));
            uint32_t antigen_id = 0;

            int result = brain_immune_present_antigen(
                immune,
                ANTIGEN_SOURCE_MANUAL,
                epitope, sizeof(epitope),
                5,       /* severity */
                0,       /* source_node */
                &antigen_id
            );

            if (result == 0) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(presenter, t);
    }
    for (auto& t : threads) {
        t.join();
    }

    /* All presentations should succeed (capacity=256, total=40) */
    EXPECT_EQ(success_count.load(), NUM_THREADS * ANTIGENS_PER_THREAD);
}

/**
 * @brief Verify brain_immune_get_antigen is thread-safe with concurrent readers
 *
 * WHAT: Multiple threads read antigens while another thread presents new ones
 * WHY:  Regression for H2 - unprotected accessor reads while writers modify
 * HOW:  1 writer thread + 3 reader threads; verify no crash
 */
TEST_F(ImmuneAntigenRaceTest, ConcurrentGetAntigenWhilePresenting_NoCrash) {
    /* Pre-populate a few antigens */
    uint32_t known_ids[5];
    for (int i = 0; i < 5; i++) {
        uint8_t epitope[4] = {(uint8_t)i, 0, 0, 0};
        brain_immune_present_antigen(
            immune, ANTIGEN_SOURCE_MANUAL,
            epitope, sizeof(epitope),
            3, 0, &known_ids[i]
        );
    }

    std::atomic<bool> running{true};

    /* Writer: keep presenting new antigens */
    auto writer = [&]() {
        int counter = 10;
        while (running.load(std::memory_order_relaxed) && counter < 50) {
            uint8_t epitope[4] = {(uint8_t)counter, 0, 0, 0};
            uint32_t id = 0;
            brain_immune_present_antigen(
                immune, ANTIGEN_SOURCE_MANUAL,
                epitope, sizeof(epitope),
                2, 0, &id
            );
            counter++;
        }
    };

    /* Readers: repeatedly query known antigens */
    auto reader = [&](int thread_id) {
        for (int i = 0; i < 50; i++) {
            uint32_t idx = (uint32_t)((thread_id + i) % 5);
            const brain_antigen_t* ag = brain_immune_get_antigen(immune, known_ids[idx]);
            /* May or may not find it -- we just check no crash */
            (void)ag;

            /* Also test is_neutralized accessor (was also unprotected) */
            bool neutralized = brain_immune_is_neutralized(immune, known_ids[idx]);
            (void)neutralized;
        }
    };

    std::thread writer_thread(writer);
    std::vector<std::thread> readers;
    for (int r = 0; r < 3; r++) {
        readers.emplace_back(reader, r);
    }

    /* Let them run briefly */
    for (auto& r : readers) {
        r.join();
    }
    running.store(false, std::memory_order_relaxed);
    writer_thread.join();

    /* If we got here without crash, the fix works */
    SUCCEED();
}

/**
 * @brief Verify antigen accessor returns correct data after concurrent writes
 *
 * WHAT: Present antigen, then verify get_antigen returns consistent data
 * WHY:  Ensures mutex protects antigen field reads
 * HOW:  Single-threaded correctness after concurrent population
 */
TEST_F(ImmuneAntigenRaceTest, AntigenAccessorReturnsCorrectData) {
    uint8_t epitope[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t antigen_id = 0;

    int result = brain_immune_present_antigen(
        immune, ANTIGEN_SOURCE_BBB,
        epitope, sizeof(epitope),
        8, 42, &antigen_id
    );
    ASSERT_EQ(result, 0);
    ASSERT_GT(antigen_id, 0u);

    const brain_antigen_t* ag = brain_immune_get_antigen(immune, antigen_id);
    ASSERT_NE(ag, nullptr);
    EXPECT_EQ(ag->source, ANTIGEN_SOURCE_BBB);
    EXPECT_EQ(ag->severity, 8u);
    EXPECT_FALSE(ag->neutralized);
}

/* ============================================================================
 * Bug C3: omni_sensory_update Race Condition Regression Tests
 * ============================================================================ */

class OmniSensoryRaceTest : public ::testing::Test {
protected:
    omni_sensory_bridge_t* bridge = nullptr;

    void SetUp() override {
        omni_sensory_config_t config;
        omni_sensory_default_config(&config);
        config.enable_binding = true;
        config.enable_attention = true;
        config.enable_bio_async = false; /* No bio-router needed for unit test */
        bridge = omni_sensory_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            omni_sensory_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/**
 * @brief Verify omni_sensory_update is safe with NULL bridge
 *
 * WHAT: NULL bridge should return error, not crash
 * WHY:  Defensive programming check
 */
TEST_F(OmniSensoryRaceTest, UpdateWithNullBridge_ReturnsError) {
    /* Should not crash, should return error */
    int result = omni_sensory_update(nullptr);
    EXPECT_NE(result, 0);
}

/**
 * @brief Verify omni_sensory_update correctly computes binding without deadlock
 *
 * WHAT: Call update (which internally calls binding) without deadlocking
 * WHY:  Regression for C3 - old code did unlock/relock around compute_binding
 * HOW:  Call update multiple times; if it returns, no deadlock occurred
 */
TEST_F(OmniSensoryRaceTest, UpdateWithBindingEnabled_NoDeadlock) {
    for (int i = 0; i < 10; i++) {
        int result = omni_sensory_update(bridge);
        EXPECT_EQ(result, 0) << "omni_sensory_update failed on iteration " << i;
    }
}

/**
 * @brief Verify concurrent sensory updates do not crash
 *
 * WHAT: Multiple threads call omni_sensory_update simultaneously
 * WHY:  Regression for C3 - concurrent update race condition
 * HOW:  4 threads each call update 20 times
 */
TEST_F(OmniSensoryRaceTest, ConcurrentUpdates_NoCrash) {
    const int NUM_THREADS = 4;
    const int UPDATES_PER_THREAD = 20;
    std::atomic<int> success_count{0};

    auto updater = [&]() {
        for (int i = 0; i < UPDATES_PER_THREAD; i++) {
            int result = omni_sensory_update(bridge);
            if (result == 0) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back(updater);
    }
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * UPDATES_PER_THREAD);
}

/**
 * @brief Verify stats are consistent after concurrent updates
 *
 * WHAT: After many updates, total_updates should match call count
 * WHY:  Ensures stats are protected by the mutex
 */
TEST_F(OmniSensoryRaceTest, StatsConsistentAfterUpdates) {
    const int NUM_UPDATES = 50;
    for (int i = 0; i < NUM_UPDATES; i++) {
        omni_sensory_update(bridge);
    }

    omni_sensory_stats_t stats;
    int result = omni_sensory_get_stats(bridge, &stats);
    ASSERT_EQ(result, 0);
    EXPECT_EQ(stats.total_updates, (uint64_t)NUM_UPDATES);
}

/* ============================================================================
 * Bug C2: SNN surrogate_backward NULL Deref Regression Tests
 * ============================================================================ */

class SNNSurrogateNullTest : public ::testing::Test {
protected:
    snn_training_ctx_t* ctx = nullptr;

    void SetUp() override {
        snn_surrogate_config_t config;
        snn_surrogate_config_default(&config);
        ctx = snn_training_create_surrogate(&config, 10, 10);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            snn_training_destroy(ctx);
            ctx = nullptr;
        }
    }
};

/**
 * @brief Verify surrogate_backward returns error (not crash) with NULL ctx
 *
 * WHAT: NULL context should return SNN_ERROR_NULL_POINTER
 * WHY:  Regression for C2 - NULL deref prevention
 */
TEST_F(SNNSurrogateNullTest, NullCtx_ReturnsError) {
    float output_grad[4] = {1.0f, 0.5f, -0.3f, 0.2f};
    float membrane_v[4] = {0.8f, 1.2f, 0.5f, 0.9f};
    float input_grad[4];

    int result = snn_surrogate_backward(nullptr, output_grad, membrane_v, 4, input_grad);
    EXPECT_EQ(result, SNN_ERROR_NULL_POINTER);
}

/**
 * @brief Verify surrogate_backward returns error with NULL gradient buffer
 *
 * WHAT: NULL output_grad (no forward pass) should return error
 * WHY:  Core fix for C2 - this was the exact crash scenario
 */
TEST_F(SNNSurrogateNullTest, NullGradientBuffer_ReturnsError) {
    float membrane_v[4] = {0.8f, 1.2f, 0.5f, 0.9f};
    float input_grad[4];

    int result = snn_surrogate_backward(ctx, nullptr, membrane_v, 4, input_grad);
    EXPECT_EQ(result, SNN_ERROR_NULL_POINTER);
}

/**
 * @brief Verify surrogate_backward returns error with NULL membrane_v
 *
 * WHAT: NULL membrane potential should return error
 * WHY:  Complete NULL coverage for all required parameters
 */
TEST_F(SNNSurrogateNullTest, NullMembraneV_ReturnsError) {
    float output_grad[4] = {1.0f, 0.5f, -0.3f, 0.2f};
    float input_grad[4];

    int result = snn_surrogate_backward(ctx, output_grad, nullptr, 4, input_grad);
    EXPECT_EQ(result, SNN_ERROR_NULL_POINTER);
}

/**
 * @brief Verify surrogate_backward returns error with NULL input_grad
 *
 * WHAT: NULL output buffer should return error
 * WHY:  Complete NULL coverage for all required parameters
 */
TEST_F(SNNSurrogateNullTest, NullInputGrad_ReturnsError) {
    float output_grad[4] = {1.0f, 0.5f, -0.3f, 0.2f};
    float membrane_v[4] = {0.8f, 1.2f, 0.5f, 0.9f};

    int result = snn_surrogate_backward(ctx, output_grad, membrane_v, 4, nullptr);
    EXPECT_EQ(result, SNN_ERROR_NULL_POINTER);
}

/**
 * @brief Verify surrogate_backward works correctly with valid inputs
 *
 * WHAT: Valid inputs should produce finite gradient values
 * WHY:  Ensure fix didn't break normal operation
 */
TEST_F(SNNSurrogateNullTest, ValidInputs_ProducesFiniteGradients) {
    float output_grad[4] = {1.0f, 0.5f, -0.3f, 0.2f};
    float membrane_v[4] = {0.8f, 1.2f, 0.5f, 0.9f};
    float input_grad[4] = {0.0f};

    int result = snn_surrogate_backward(ctx, output_grad, membrane_v, 4, input_grad);
    EXPECT_EQ(result, SNN_SUCCESS);

    /* All output gradients should be finite */
    for (int i = 0; i < 4; i++) {
        EXPECT_TRUE(std::isfinite(input_grad[i]))
            << "input_grad[" << i << "] = " << input_grad[i] << " is not finite";
    }
}

/**
 * @brief Verify gradients are within clip bounds
 *
 * WHAT: Surrogate backward clips gradients to [-5, 5]
 * WHY:  Ensure gradient clipping is applied correctly
 */
TEST_F(SNNSurrogateNullTest, GradientsAreClipped) {
    /* Large output gradients that could cause explosion */
    float output_grad[2] = {100.0f, -100.0f};
    float membrane_v[2] = {1.0f, 1.0f}; /* At threshold: max surrogate gradient */
    float input_grad[2] = {0.0f};

    int result = snn_surrogate_backward(ctx, output_grad, membrane_v, 2, input_grad);
    EXPECT_EQ(result, SNN_SUCCESS);

    for (int i = 0; i < 2; i++) {
        EXPECT_GE(input_grad[i], -5.0f)
            << "input_grad[" << i << "] below clip minimum";
        EXPECT_LE(input_grad[i], 5.0f)
            << "input_grad[" << i << "] above clip maximum";
    }
}
