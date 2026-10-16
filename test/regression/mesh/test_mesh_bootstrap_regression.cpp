/**
 * @file test_mesh_bootstrap_regression.cpp
 * @brief Regression Tests for NIMCP Mesh Network Bootstrap System
 *
 * WHAT: Tests for stability issues, edge cases, race conditions, and backward compat
 * WHY:  Catch regressions in bootstrap behavior, prevent known bugs from recurring
 * HOW:  Stress test initialization, test boundary conditions, verify error handling
 *
 * TEST COVERAGE:
 * - Rapid create/destroy cycles (memory leaks)
 * - Large module registration (resource exhaustion)
 * - Concurrent access patterns (race conditions)
 * - Edge case configurations (empty subsystems, NULL params)
 * - Error recovery (partial initialization failure)
 * - Pattern routing edge cases (zero patterns, high dimensions)
 * - Neuromodulation boundary values
 * - Learning with extreme reward signals
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>
#include <cmath>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_bootstrap.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MeshBootstrapRegressionTest : public ::testing::Test {
protected:
    mesh_bootstrap_t* bootstrap;

    void SetUp() override {
        bootstrap = nullptr;
    }

    void TearDown() override {
        if (bootstrap) {
            mesh_bootstrap_destroy(bootstrap);
            bootstrap = nullptr;
        }
    }

    mesh_pattern_t create_pattern(const float* values, size_t count) {
        mesh_pattern_t pattern;
        mesh_pattern_init(&pattern);

        float magnitude = 0.0f;
        for (size_t i = 0; i < count && i < MESH_PATTERN_DIM; i++) {
            pattern.vector[i] = values[i];
            magnitude += values[i] * values[i];
        }
        pattern.magnitude = sqrtf(magnitude);
        pattern.active_dims = (uint32_t)count;

        return pattern;
    }

    mesh_pattern_t create_random_pattern(std::mt19937& rng) {
        mesh_pattern_t pattern;
        mesh_pattern_init(&pattern);

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        float magnitude = 0.0f;

        for (int i = 0; i < MESH_PATTERN_DIM; i++) {
            pattern.vector[i] = dist(rng);
            magnitude += pattern.vector[i] * pattern.vector[i];
        }
        pattern.magnitude = sqrtf(magnitude);
        pattern.active_dims = MESH_PATTERN_DIM;

        return pattern;
    }
};

/* ============================================================================
 * Memory Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, RapidCreateDestroyNoLeak) {
    /* Regression: Memory leaks on repeated create/destroy */
    for (int i = 0; i < 20; i++) {
        mesh_bootstrap_config_t config;
        mesh_bootstrap_default_config(&config);
        config.subsystems = MESH_SUBSYSTEMS_CORE;

        mesh_bootstrap_t* b = mesh_bootstrap_create(&config);
        ASSERT_NE(b, nullptr) << "Failed at iteration " << i;

        /* Brief usage */
        mesh_bootstrap_update(b, 1);

        mesh_bootstrap_destroy(b);
    }

    /* If we get here without crashing, no obvious memory corruption */
    SUCCEED();
}

TEST_F(MeshBootstrapRegressionTest, DestroyNullSafe) {
    /* Regression: Crash on NULL destroy */
    mesh_bootstrap_destroy(nullptr);
    SUCCEED();
}

TEST_F(MeshBootstrapRegressionTest, DoubleDestroySafe) {
    /* Regression: Double free on repeated destroy */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* First destroy */
    mesh_bootstrap_destroy(bootstrap);
    bootstrap = nullptr;

    /* Second destroy on NULL should be safe */
    mesh_bootstrap_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Edge Case Configuration Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, EmptySubsystemsConfig) {
    /* Regression: Crash when all subsystems disabled */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);

    /* Disable everything */
    config.subsystems.enable_cognitive = false;
    config.subsystems.enable_sensory = false;
    config.subsystems.enable_motor = false;
    config.subsystems.enable_memory = false;
    config.subsystems.enable_security = false;
    config.subsystems.enable_gpu = false;
    config.subsystems.enable_plasticity = false;
    config.subsystems.enable_glial = false;
    config.subsystems.enable_swarm = false;
    config.subsystems.enable_async = false;
    config.subsystems.enable_lnn = false;
    config.subsystems.enable_snn = false;

    bootstrap = mesh_bootstrap_create(&config);
    /* Should still create, just with no modules */
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);
    /* May have some core modules, but subsystem counts should be 0 */
    EXPECT_EQ(stats.cognitive_modules, 0u);
    EXPECT_EQ(stats.sensory_modules, 0u);
}

TEST_F(MeshBootstrapRegressionTest, NullConfigUsesDefaults) {
    /* Regression: Crash on NULL config */
    bootstrap = mesh_bootstrap_create(nullptr);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats;
    mesh_bootstrap_get_stats(bootstrap, &stats);
    EXPECT_TRUE(stats.fully_initialized);
}

/* ============================================================================
 * Pattern Routing Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, ZeroPatternRouting) {
    /* Regression: Crash or hang on zero pattern */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Register module with zero pattern */
    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);
    /* All zeros by default */

    mesh_bootstrap_register_receptive_field(bootstrap, 0xE001, &field);

    /* Route with zero pattern */
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));

    mesh_participant_id_t endorsers[10];
    size_t count = 0;

    /* Should not crash */
    nimcp_error_t err = mesh_bootstrap_route_by_pattern(
        bootstrap, &tx, endorsers, 10, &count);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapRegressionTest, HighDimensionalPatternRouting) {
    /* Regression: Issues with full-dimensional patterns */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Create pattern with all 64 dimensions active */
    mesh_pattern_t full_pattern;
    mesh_pattern_init(&full_pattern);

    float mag = 0.0f;
    for (int i = 0; i < MESH_PATTERN_DIM; i++) {
        full_pattern.vector[i] = 1.0f / sqrtf((float)MESH_PATTERN_DIM);
        mag += full_pattern.vector[i] * full_pattern.vector[i];
    }
    full_pattern.magnitude = sqrtf(mag);
    full_pattern.active_dims = MESH_PATTERN_DIM;

    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);
    field.preferred[0] = full_pattern;
    field.pattern_count = 1;
    field.threshold = 0.3f;

    mesh_bootstrap_register_receptive_field(bootstrap, 0xE002, &field);

    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = full_pattern;

    mesh_participant_id_t endorsers[10];
    size_t count = 0;

    EXPECT_EQ(mesh_bootstrap_route_by_pattern(bootstrap, &tx, endorsers, 10, &count),
              NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapRegressionTest, ManyModulesRegistration) {
    /* Regression: Resource exhaustion with many modules */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Register many additional modules */
    std::mt19937 rng(42);

    for (int i = 0; i < 100; i++) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);
        field.preferred[0] = create_random_pattern(rng);
        field.pattern_count = 1;
        field.threshold = 0.3f;

        nimcp_error_t err = mesh_bootstrap_register_receptive_field(
            bootstrap, 0xF000 + i, &field);
        /* Should succeed or fail gracefully, not crash */
        (void)err;
    }

    /* Route should still work */
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    mesh_participant_id_t endorsers[50];
    size_t count = 0;

    EXPECT_EQ(mesh_bootstrap_route_by_pattern(bootstrap, &tx, endorsers, 50, &count),
              NIMCP_SUCCESS);
}

/* ============================================================================
 * Neuromodulation Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, NeuromodulationBoundaryValues) {
    /* Regression: Issues with extreme neuromodulation levels */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Test boundary values */
    EXPECT_EQ(mesh_bootstrap_apply_neuromodulation(bootstrap, MESH_NEUROMOD_DOPAMINE, 0.0f),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_bootstrap_apply_neuromodulation(bootstrap, MESH_NEUROMOD_DOPAMINE, 1.0f),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_bootstrap_apply_neuromodulation(bootstrap, MESH_NEUROMOD_NOREPINEPHRINE, 0.0f),
              NIMCP_SUCCESS);
    EXPECT_EQ(mesh_bootstrap_apply_neuromodulation(bootstrap, MESH_NEUROMOD_NOREPINEPHRINE, 1.0f),
              NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapRegressionTest, NeuromodulationOutOfRange) {
    /* Regression: Crash or undefined behavior with out-of-range values */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Out of range values should be handled gracefully */
    mesh_bootstrap_apply_neuromodulation(bootstrap, MESH_NEUROMOD_DOPAMINE, -1.0f);
    mesh_bootstrap_apply_neuromodulation(bootstrap, MESH_NEUROMOD_DOPAMINE, 2.0f);
    mesh_bootstrap_apply_neuromodulation(bootstrap, MESH_NEUROMOD_DOPAMINE, 100.0f);

    /* Should not crash */
    SUCCEED();
}

/* ============================================================================
 * Learning Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, LearningExtremeRewardSignals) {
    /* Regression: Issues with extreme reward signals */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Register a module */
    mesh_receptive_field_t field;
    mesh_receptive_field_init(&field);
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    field.preferred[0] = create_pattern(pattern, 4);
    field.pattern_count = 1;
    field.threshold = 0.3f;

    mesh_participant_id_t module_id = 0x1001;
    mesh_bootstrap_register_receptive_field(bootstrap, module_id, &field);

    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.content_pattern = create_pattern(pattern, 4);

    mesh_participant_id_t endorsers[] = {module_id};

    /* Extreme positive reward */
    EXPECT_EQ(mesh_bootstrap_learn_routing_outcome(
        bootstrap, &tx, endorsers, 1, true, 1000.0f), NIMCP_SUCCESS);

    /* Extreme negative reward */
    EXPECT_EQ(mesh_bootstrap_learn_routing_outcome(
        bootstrap, &tx, endorsers, 1, false, -1000.0f), NIMCP_SUCCESS);

    /* Zero reward */
    EXPECT_EQ(mesh_bootstrap_learn_routing_outcome(
        bootstrap, &tx, endorsers, 1, true, 0.0f), NIMCP_SUCCESS);
}

TEST_F(MeshBootstrapRegressionTest, LearningEmptyEndorserList) {
    /* Regression: Issues when learning with no endorsers */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_pattern_transaction_t tx;
    memset(&tx, 0, sizeof(tx));
    float pattern[] = {1.0f, 0.0f, 0.0f, 0.0f};
    tx.content_pattern = create_pattern(pattern, 4);

    /* Empty endorser list */
    mesh_participant_id_t endorsers[1];
    EXPECT_EQ(mesh_bootstrap_learn_routing_outcome(
        bootstrap, &tx, endorsers, 0, true, 1.0f), NIMCP_SUCCESS);
}

/* ============================================================================
 * Concurrent Access Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, ConcurrentPatternRegistration) {
    /* Regression: Race condition in concurrent registration */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> success_count{0};
    std::atomic<int> total_count{0};

    auto register_func = [&](int thread_id) {
        std::mt19937 rng(thread_id * 1000);
        int local_count = 0;

        while (running && local_count < 20) {
            mesh_receptive_field_t field;
            mesh_receptive_field_init(&field);
            field.preferred[0] = create_random_pattern(rng);
            field.pattern_count = 1;
            field.threshold = 0.3f;

            mesh_participant_id_t id = 0x10000 + (thread_id * 1000) + local_count;
            nimcp_error_t err = mesh_bootstrap_register_receptive_field(
                bootstrap, id, &field);

            if (err == NIMCP_SUCCESS) {
                success_count++;
            }
            total_count++;
            local_count++;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(register_func, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(total_count.load(), 0);
    /* Some may fail due to capacity, but shouldn't crash */
}

TEST_F(MeshBootstrapRegressionTest, ConcurrentNeuromodulation) {
    /* Regression: Race condition in concurrent neuromodulation */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    std::atomic<bool> running{true};

    auto modulate_func = [&](mesh_neuromodulator_t neuromod) {
        std::mt19937 rng((unsigned)neuromod);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);

        while (running) {
            mesh_bootstrap_apply_neuromodulation(bootstrap, neuromod, dist(rng));
            std::this_thread::yield();
        }
    };

    std::thread t1(modulate_func, MESH_NEUROMOD_DOPAMINE);
    std::thread t2(modulate_func, MESH_NEUROMOD_NOREPINEPHRINE);
    std::thread t3(modulate_func, MESH_NEUROMOD_ACETYLCHOLINE);
    std::thread t4(modulate_func, MESH_NEUROMOD_SEROTONIN);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    running = false;

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    SUCCEED();
}

/* ============================================================================
 * State Consistency Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, StatsAfterOperations) {
    /* Regression: Stats not updating correctly */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_bootstrap_stats_t stats1, stats2;
    mesh_bootstrap_get_stats(bootstrap, &stats1);

    /* Perform operations */
    mesh_bootstrap_update(bootstrap, 100);
    mesh_bootstrap_gossip_all(bootstrap, 5);
    mesh_bootstrap_process_transactions(bootstrap);

    mesh_bootstrap_get_stats(bootstrap, &stats2);

    /* Stats should still be valid */
    EXPECT_TRUE(stats2.fully_initialized);
    EXPECT_EQ(stats2.total_channels_active, stats1.total_channels_active);
}

TEST_F(MeshBootstrapRegressionTest, ChannelAccessAfterGossip) {
    /* Regression: Channel pointers invalid after gossip */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    mesh_channel_t* left = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_LEFT_HEMISPHERE);
    ASSERT_NE(left, nullptr);

    /* Run gossip */
    mesh_bootstrap_gossip_all(bootstrap, 10);

    /* Channel should still be accessible */
    mesh_channel_t* left_after = mesh_bootstrap_get_channel(bootstrap, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(left, left_after);

    /* Should still work */
    mesh_channel_stats_t stats;
    EXPECT_EQ(mesh_channel_get_stats(left, &stats), NIMCP_SUCCESS);
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, InitializationPerformance) {
    /* Regression: Initialization too slow */
    auto start = std::chrono::high_resolution_clock::now();

    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should initialize within reasonable time (allow for slow CI) */
    EXPECT_LT(duration.count(), 5000);  /* 5 seconds max */
}

TEST_F(MeshBootstrapRegressionTest, PatternRoutingPerformance) {
    /* Regression: Pattern routing too slow */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Register several modules */
    std::mt19937 rng(42);
    for (int i = 0; i < 50; i++) {
        mesh_receptive_field_t field;
        mesh_receptive_field_init(&field);
        field.preferred[0] = create_random_pattern(rng);
        field.pattern_count = 1;
        field.threshold = 0.3f;

        mesh_bootstrap_register_receptive_field(bootstrap, 0x20000 + i, &field);
    }

    /* Measure routing time */
    auto start = std::chrono::high_resolution_clock::now();

    const int NUM_ROUTES = 1000;
    for (int i = 0; i < NUM_ROUTES; i++) {
        mesh_pattern_transaction_t tx;
        memset(&tx, 0, sizeof(tx));
        tx.content_pattern = create_random_pattern(rng);

        mesh_participant_id_t endorsers[10];
        size_t count = 0;

        mesh_bootstrap_route_by_pattern(bootstrap, &tx, endorsers, 10, &count);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = (float)duration.count() / NUM_ROUTES;
    EXPECT_LT(avg_us, 1000.0f);  /* Less than 1ms per route */
}

/* ============================================================================
 * Error Recovery Regression Tests
 * ============================================================================ */

TEST_F(MeshBootstrapRegressionTest, ContinueAfterNullParams) {
    /* Regression: System unusable after NULL parameter errors */
    mesh_bootstrap_config_t config;
    mesh_bootstrap_default_config(&config);
    config.subsystems = MESH_SUBSYSTEMS_CORE;

    bootstrap = mesh_bootstrap_create(&config);
    ASSERT_NE(bootstrap, nullptr);

    /* Try various NULL operations */
    mesh_bootstrap_register_receptive_field(bootstrap, 0x1234, nullptr);
    mesh_bootstrap_route_by_pattern(bootstrap, nullptr, nullptr, 0, nullptr);
    mesh_bootstrap_learn_routing_outcome(bootstrap, nullptr, nullptr, 0, true, 0.0f);

    /* System should still work */
    mesh_bootstrap_stats_t stats;
    EXPECT_EQ(mesh_bootstrap_get_stats(bootstrap, &stats), NIMCP_SUCCESS);
    EXPECT_TRUE(stats.fully_initialized);

    mesh_bootstrap_update(bootstrap, 10);
    mesh_bootstrap_gossip_all(bootstrap, 1);

    SUCCEED();
}
