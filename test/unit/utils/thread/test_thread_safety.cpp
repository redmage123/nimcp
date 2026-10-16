/**
 * @file test_thread_safety.cpp
 * @brief Thread safety tests for NIMCP concurrent operations
 *
 * WHAT: Tests concurrent access to neuromodulator and BCM systems
 * WHY:  Verify thread-safe operations under high contention
 *
 * DESIGN PATTERNS:
 * - Builder Pattern: Construct complex concurrent test scenarios
 * - Template Method: Reusable concurrent test harness
 * - Strategy Pattern: Different contention strategies
 *
 * TEST PHILOSOPHY:
 * - TDD: Tests written before implementation
 * - Concurrent stress testing: Many threads, many operations
 * - Race detection: Use thread sanitizer to catch bugs
 * - Correctness: Verify final state matches sequential execution
 *
 * @author NIMCP Development Team
 * @date 2025-11-01
 */

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include <pthread.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Thread safety test fixture for neuromodulators
 *
 * WHAT: Provides concurrent testing infrastructure
 * WHY:  Setup/teardown for multi-threaded tests
 */
class NeuromodulatorThreadSafetyTest : public ::testing::Test {
protected:
    neuromodulator_system_t system;
    static constexpr uint32_t NUM_THREADS = 8;
    static constexpr uint32_t OPS_PER_THREAD = 10000;

    void SetUp() override {
        /* WHAT: Create neuromodulator system with default config
         * WHY:  Clean state for each test
         */
        system = neuromodulator_system_create(nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        /* WHAT: Clean up neuromodulator system
         * WHY:  Prevent memory leaks
         */
        neuromodulator_system_destroy(system);
    }
};

/**
 * @brief Thread safety test fixture for BCM
 *
 * WHAT: Provides concurrent testing infrastructure for BCM
 * WHY:  Setup/teardown for multi-threaded BCM tests
 */
class BCMThreadSafetyTest : public ::testing::Test {
protected:
    static constexpr uint32_t NUM_SYNAPSES = 1000;
    static constexpr uint32_t NUM_THREADS = 8;
    static constexpr uint32_t OPS_PER_THREAD = 1000;

    std::vector<bcm_synapse_t> synapses;
    bcm_params_t params;

    void SetUp() override {
        /* WHAT: Initialize synapse array
         * WHY:  Provide shared state for concurrent tests
         */
        synapses.resize(NUM_SYNAPSES);
        for (uint32_t i = 0; i < NUM_SYNAPSES; i++) {
            synapses[i] = bcm_synapse_init(0.5f, 0.5f);
        }
        params = bcm_params_cortical();
    }
};

//=============================================================================
// Neuromodulator Thread Safety Tests
//=============================================================================

/**
 * @brief Test concurrent update() calls
 *
 * WHAT: Multiple threads call neuromodulator_update() simultaneously
 * WHY:  Verify concentration updates are atomic and consistent
 *
 * PATTERN: Template Method (concurrent test harness)
 */
TEST_F(NeuromodulatorThreadSafetyTest, ConcurrentUpdate) {
    /* WHAT: Launch multiple threads updating concentrations
     * WHY:  Test for race conditions in update path
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        neuromodulator_system_t sys;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start;
        uint32_t ops;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        /* WHAT: Signal ready and wait for start
         * WHY:  Synchronize all threads to maximize contention
         */
        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start->load(std::memory_order_acquire)) {
            // Spin wait
        }

        /* WHAT: Perform many concurrent updates
         * WHY:  Stress test mutex protection
         */
        for (uint32_t i = 0; i < args->ops; i++) {
            neuromodulator_update(args->sys, 0.001f);
        }

        return nullptr;
    };

    /* WHAT: Create thread arguments
     * WHY:  Pass shared system to all threads
     */
    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {system, &ready_count, &start, OPS_PER_THREAD};
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    /* WHAT: Wait for all threads ready, then start
     * WHY:  Maximize concurrent access
     */
    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    /* WHAT: Join all threads
     * WHY:  Wait for completion before verification
     */
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Verify system is still in valid state
     * WHY:  Race conditions would corrupt state
     */
    neuromodulator_pool_t pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(system, &pool));

    /* WHAT: Verify concentrations are in valid range
     * WHY:  Corruption would cause out-of-bounds values
     */
    EXPECT_GE(neuromodulator_pool_get_dopamine(&pool), 0.0f);
    EXPECT_LE(neuromodulator_pool_get_dopamine(&pool), 1.0f);
    EXPECT_GE(neuromodulator_pool_get_serotonin(&pool), 0.0f);
    EXPECT_LE(neuromodulator_pool_get_serotonin(&pool), 1.0f);

    /* WHAT: Verify update count matches expected
     * WHY:  Lost updates would cause lower count
     */
    neuromodulator_stats_t stats;
    ASSERT_TRUE(neuromodulator_get_stats(system, &stats));
    EXPECT_EQ(pool.last_update, NUM_THREADS * OPS_PER_THREAD);
}

/**
 * @brief Test concurrent release calls
 *
 * WHAT: Multiple threads release dopamine simultaneously
 * WHY:  Verify release operations are atomic
 */
TEST_F(NeuromodulatorThreadSafetyTest, ConcurrentRelease) {
    /* WHAT: Multiple threads releasing dopamine
     * WHY:  Test for lost increments in statistics
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        neuromodulator_system_t sys;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start;
        uint32_t ops;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start->load(std::memory_order_acquire)) {
            // Spin wait
        }

        /* WHAT: Repeatedly release dopamine
         * WHY:  Stress test atomic counter increments
         */
        for (uint32_t i = 0; i < args->ops; i++) {
            neuromodulator_release_dopamine(args->sys, 1.0f, 0.5f);
        }

        return nullptr;
    };

    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {system, &ready_count, &start, OPS_PER_THREAD};
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Verify release count is exact
     * WHY:  Lost increments would show lower count
     */
    neuromodulator_stats_t stats;
    ASSERT_TRUE(neuromodulator_get_stats(system, &stats));
    EXPECT_EQ(stats.dopamine_releases, NUM_THREADS * OPS_PER_THREAD);
    // Note: rpe_count field doesn't exist in neuromodulator_stats_t
    // TODO: Add rpe_count tracking if needed
}

/**
 * @brief Test concurrent read/write access
 *
 * WHAT: Multiple readers and writers accessing system
 * WHY:  Verify reader-writer lock correctness
 */
TEST_F(NeuromodulatorThreadSafetyTest, ConcurrentReadWrite) {
    /* WHAT: Half threads read, half write
     * WHY:  Test reader-writer lock performance
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        neuromodulator_system_t sys;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start;
        uint32_t ops;
        bool is_writer;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start->load(std::memory_order_acquire)) {
            // Spin wait
        }

        if (args->is_writer) {
            /* WHAT: Writer threads update system
             * WHY:  Test write-side locking
             */
            for (uint32_t i = 0; i < args->ops; i++) {
                neuromodulator_update(args->sys, 0.001f);
                neuromodulator_release_dopamine(args->sys, 1.0f, 0.5f);
            }
        } else {
            /* WHAT: Reader threads get statistics
             * WHY:  Test read-side locking (should not block readers)
             */
            for (uint32_t i = 0; i < args->ops; i++) {
                neuromodulator_pool_t pool = {};
                neuromodulator_get_levels(args->sys, &pool);

                neuromodulator_stats_t stats;
                neuromodulator_get_stats(args->sys, &stats);
            }
        }

        return nullptr;
    };

    /* WHAT: Create half writers, half readers
     * WHY:  Realistic mixed workload
     */
    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {system, &ready_count, &start, OPS_PER_THREAD, i < NUM_THREADS / 2};
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Verify final state is consistent
     * WHY:  No crashes or corruption indicates success
     */
    neuromodulator_stats_t stats;
    ASSERT_TRUE(neuromodulator_get_stats(system, &stats));

    /* WHAT: Verify expected number of writer operations
     * WHY:  Readers should not interfere with writers
     */
    uint32_t expected_updates = (NUM_THREADS / 2) * OPS_PER_THREAD;
    neuromodulator_pool_t pool = {};
    neuromodulator_get_levels(system, &pool);
    EXPECT_EQ(pool.last_update, expected_updates);
}

/**
 * @brief Test effect pool thread safety
 *
 * WHAT: Multiple threads acquiring/releasing effect buffers
 * WHY:  Verify object pool thread safety
 */
TEST_F(NeuromodulatorThreadSafetyTest, EffectPoolConcurrent) {
    /* WHAT: Concurrent effect computation
     * WHY:  Test thread-local or locked pool
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<uint32_t> success_count{0};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        neuromodulator_system_t sys;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start;
        std::atomic<uint32_t>* successes;
        uint32_t ops;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start->load(std::memory_order_acquire)) {
            // Spin wait
        }

        /* WHAT: Repeatedly compute effects
         * WHY:  Stress test effect pool allocation
         */
        receptor_profile_t receptors = neuromodulator_profile_cortical_excitatory();

        for (uint32_t i = 0; i < args->ops; i++) {
            modulation_effects_t effects;
            if (neuromodulator_compute_effects(args->sys, &receptors, &effects)) {
                args->successes->fetch_add(1, std::memory_order_relaxed);
            }
        }

        return nullptr;
    };

    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {system, &ready_count, &start, &success_count, OPS_PER_THREAD};
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Verify all operations succeeded
     * WHY:  Thread-local pools should never fail
     */
    EXPECT_EQ(success_count.load(), NUM_THREADS * OPS_PER_THREAD);
}

//=============================================================================
// BCM Thread Safety Tests
//=============================================================================

/**
 * @brief Test concurrent BCM rule application
 *
 * WHAT: Multiple threads updating same synapse
 * WHY:  Verify spinlock protection (worst case)
 */
TEST_F(BCMThreadSafetyTest, ConcurrentSingleSynapse) {
    /* WHAT: All threads hammer same synapse
     * WHY:  Maximum contention scenario
     */

    bcm_synapse_t shared_synapse = bcm_synapse_init(0.5f, 0.5f);

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        bcm_synapse_t* synapse;
        bcm_params_t* params;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start;
        uint32_t ops;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start->load(std::memory_order_acquire)) {
            // Spin wait
        }

        /* WHAT: Repeatedly update same synapse
         * WHY:  Stress test spinlock under contention
         */
        for (uint32_t i = 0; i < args->ops; i++) {
            float pre = 0.8f;
            float post = 0.7f;
            bcm_update_threshold(args->synapse, post, 1.0f, args->params);
            bcm_apply_rule(args->synapse, pre, post, 1.0f, args->params);
        }

        return nullptr;
    };

    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {&shared_synapse, &params, &ready_count, &start, OPS_PER_THREAD};
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Verify synapse state is valid
     * WHY:  Race conditions would corrupt state
     */
    EXPECT_GE(shared_synapse.weight, 0.0f);
    EXPECT_LE(shared_synapse.weight, 1.0f);
    EXPECT_GT(shared_synapse.threshold, 0.0f);
}

/**
 * @brief Test concurrent BCM with disjoint synapses
 *
 * WHAT: Each thread updates different synapses
 * WHY:  Verify no false sharing or unnecessary locking
 */
TEST_F(BCMThreadSafetyTest, ConcurrentDisjointSynapses) {
    /* WHAT: Partition synapses among threads
     * WHY:  Test lock-free operation when no sharing
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        bcm_synapse_t* synapses;
        bcm_params_t* params;
        uint32_t start_idx;
        uint32_t count;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start_flag;
        uint32_t ops;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start_flag->load(std::memory_order_acquire)) {
            // Spin wait
        }

        /* WHAT: Update only assigned synapses
         * WHY:  No contention, should be very fast
         */
        for (uint32_t i = 0; i < args->ops; i++) {
            for (uint32_t j = 0; j < args->count; j++) {
                bcm_synapse_t* syn = &args->synapses[args->start_idx + j];
                float pre = 0.8f;
                float post = 0.7f;
                bcm_update_threshold(syn, post, 1.0f, args->params);
                bcm_apply_rule(syn, pre, post, 1.0f, args->params);
            }
        }

        return nullptr;
    };

    /* WHAT: Divide synapses evenly among threads
     * WHY:  Ensure disjoint access patterns
     */
    uint32_t synapses_per_thread = NUM_SYNAPSES / NUM_THREADS;
    std::vector<ThreadArgs> thread_args(NUM_THREADS);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {
            synapses.data(),
            &params,
            i * synapses_per_thread,
            synapses_per_thread,
            &ready_count,
            &start,
            OPS_PER_THREAD
        };
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }

    /* WHAT: Measure execution time
     * WHY:  Should be nearly linear speedup (no contention)
     */
    auto start_time = std::chrono::high_resolution_clock::now();
    start.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }
    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time).count();

    /* WHAT: Verify all synapses were updated
     * WHY:  Sanity check for correctness
     */
    for (const auto& syn : synapses) {
        EXPECT_GE(syn.weight, 0.0f);
        EXPECT_LE(syn.weight, 1.0f);
    }

    /* WHAT: Performance check (optional)
     * WHY:  Ensure parallel execution is actually faster
     * NOTE: With 8 threads, expect ~5-6x speedup (accounting for overhead)
     */
    std::cout << "Disjoint synapse update time: " << duration_us << " μs" << std::endl;
}

/**
 * @brief Test BCM statistics computation with concurrent updates
 *
 * WHAT: Compute stats while other threads update synapses
 * WHY:  Verify stats computation doesn't interfere with updates
 */
TEST_F(BCMThreadSafetyTest, ConcurrentStatsComputation) {
    /* WHAT: One thread computes stats, others update
     * WHY:  Test that stats reading doesn't block writers
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        bcm_synapse_t* synapses;
        uint32_t num_synapses;
        bcm_params_t* params;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start_flag;
        std::atomic<bool>* stop_flag;
        bool is_stats_thread;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start_flag->load(std::memory_order_acquire)) {
            // Spin wait
        }

        if (args->is_stats_thread) {
            /* WHAT: Continuously compute statistics
             * WHY:  Test read operation under write load
             */
            while (!args->stop_flag->load(std::memory_order_acquire)) {
                bcm_stats_t stats;
                bcm_compute_stats(args->synapses, args->num_synapses, &stats);
            }
        } else {
            /* WHAT: Update synapses continuously
             * WHY:  Generate write load
             */
            uint32_t iterations = 0;
            while (!args->stop_flag->load(std::memory_order_acquire) && iterations < 10000) {
                for (uint32_t i = 0; i < args->num_synapses; i++) {
                    float pre = 0.8f;
                    float post = 0.7f;
                    bcm_update_threshold(&args->synapses[i], post, 1.0f, args->params);
                    bcm_apply_rule(&args->synapses[i], pre, post, 1.0f, args->params);
                }
                iterations++;
            }
        }

        return nullptr;
    };

    /* WHAT: Create 1 stats thread, rest are updaters
     * WHY:  Realistic workload distribution
     */
    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {
            synapses.data(),
            NUM_SYNAPSES,
            &params,
            &ready_count,
            &start,
            &stop,
            i == 0  // First thread is stats thread
        };
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    /* WHAT: Let threads run for 100ms
     * WHY:  Sufficient time to detect race conditions
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Final stats computation
     * WHY:  Verify system is still in valid state
     */
    bcm_stats_t final_stats;
    ASSERT_TRUE(bcm_compute_stats(synapses.data(), NUM_SYNAPSES, &final_stats));
    EXPECT_GE(final_stats.avg_weight, 0.0f);
    EXPECT_LE(final_stats.avg_weight, 1.0f);
}

//=============================================================================
// Integration Tests - Neuromodulators + BCM + Learning
//=============================================================================

/**
 * @brief Integration test: Concurrent neuromodulator release + BCM learning
 *
 * WHAT: Simulate realistic scenario - dopamine release gates BCM plasticity
 * WHY:  Verify thread safety when multiple systems interact
 * PATTERN: Integration test (combines neuromodulators + BCM + threading)
 */
TEST_F(NeuromodulatorThreadSafetyTest, Integration_ConcurrentDopamineAndBCM) {
    /* WHAT: Test realistic learning scenario
     * WHY:  Dopamine-modulated learning is core NIMCP functionality
     * SCENARIO:
     * - Thread 1-4: Release dopamine based on rewards
     * - Thread 5-8: Update BCM synapses modulated by dopamine
     */

    const uint32_t NUM_SYNAPSES = 1000;
    std::vector<bcm_synapse_t> synapses(NUM_SYNAPSES);
    bcm_params_t bcm_params = bcm_params_cortical();

    // Initialize synapses
    for (uint32_t i = 0; i < NUM_SYNAPSES; i++) {
        synapses[i] = bcm_synapse_init(0.5f, 0.5f);
    }

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        neuromodulator_system_t neuro_sys;
        bcm_synapse_t* synapses;
        uint32_t num_synapses;
        bcm_params_t* params;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start;
        bool is_dopamine_thread;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start->load(std::memory_order_acquire)) {
            // Spin wait
        }

        if (args->is_dopamine_thread) {
            /* WHAT: Release dopamine based on simulated rewards
             * WHY:  Test concurrent dopamine release
             */
            for (int i = 0; i < 1000; i++) {
                float reward = (i % 10) / 10.0f;  // Variable rewards
                float predicted = 0.5f;
                neuromodulator_release_dopamine(args->neuro_sys, reward, predicted);
                neuromodulator_update(args->neuro_sys, 0.001f);
            }
        } else {
            /* WHAT: Update BCM synapses modulated by dopamine
             * WHY:  Test concurrent BCM learning with neuromodulator gating
             */
            for (int i = 0; i < 1000; i++) {
                // Get current dopamine level
                neuromodulator_pool_t pool = {};
                neuromodulator_get_levels(args->neuro_sys, &pool);

                // Update random synapse with dopamine modulation
                uint32_t syn_idx = i % args->num_synapses;
                bcm_synapse_t* syn = &args->synapses[syn_idx];

                float pre = 0.8f;
                float post = 0.7f;

                // Apply dopamine-modulated BCM learning
                bcm_apply_rule_modulated(syn, pre, post, 1.0f, args->params, neuromodulator_pool_get_dopamine(&pool));
            }
        }

        return nullptr;
    };

    /* WHAT: Create half dopamine threads, half BCM threads
     * WHY:  Realistic workload - reward processing + learning
     */
    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {
            system,
            synapses.data(),
            NUM_SYNAPSES,
            &bcm_params,
            &ready_count,
            &start,
            i < NUM_THREADS / 2  // Half dopamine, half BCM
        };
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Verify system is in valid state
     * WHY:  Integration should not corrupt either system
     */
    neuromodulator_pool_t final_pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(system, &final_pool));
    EXPECT_GE(neuromodulator_pool_get_dopamine(&final_pool), 0.0f);
    EXPECT_LE(neuromodulator_pool_get_dopamine(&final_pool), 1.0f);

    // Verify BCM synapses are valid
    for (uint32_t i = 0; i < NUM_SYNAPSES; i++) {
        EXPECT_GE(synapses[i].weight, 0.0f);
        EXPECT_LE(synapses[i].weight, 1.0f);
    }

    // Clean up BCM spinlocks
    for (uint32_t i = 0; i < NUM_SYNAPSES; i++) {
        nimcp_spinlock_destroy(&synapses[i].lock);
    }
}

/**
 * @brief Integration test: Multi-reader stats query during active learning
 *
 * WHAT: Multiple threads reading stats while others update system
 * WHY:  Common pattern - monitoring during training
 */
TEST_F(NeuromodulatorThreadSafetyTest, Integration_StatsMonitoringDuringLearning) {
    /* WHAT: Simulate monitoring dashboard reading stats during training
     * WHY:  Verify reader-writer lock allows concurrent monitoring
     */

    std::atomic<uint32_t> ready_count{0};
    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<uint32_t> stats_read_count{0};
    pthread_t threads[NUM_THREADS];

    struct ThreadArgs {
        neuromodulator_system_t sys;
        std::atomic<uint32_t>* ready;
        std::atomic<bool>* start_flag;
        std::atomic<bool>* stop_flag;
        std::atomic<uint32_t>* read_count;
        bool is_monitor;
    };

    auto thread_func = +[](void* arg) -> void* {
        ThreadArgs* args = static_cast<ThreadArgs*>(arg);

        args->ready->fetch_add(1, std::memory_order_release);
        while (!args->start_flag->load(std::memory_order_acquire)) {
            // Spin wait
        }

        if (args->is_monitor) {
            /* WHAT: Monitor thread - continuously read stats
             * WHY:  Simulate dashboard/logging
             */
            while (!args->stop_flag->load(std::memory_order_acquire)) {
                neuromodulator_stats_t stats;
                neuromodulator_pool_t pool = {};

                if (neuromodulator_get_stats(args->sys, &stats) &&
                    neuromodulator_get_levels(args->sys, &pool)) {
                    args->read_count->fetch_add(1, std::memory_order_relaxed);
                }
            }
        } else {
            /* WHAT: Learning thread - update and release
             * WHY:  Simulate active learning
             */
            for (int i = 0; i < 5000; i++) {
                neuromodulator_update(args->sys, 0.001f);
                if (i % 10 == 0) {
                    neuromodulator_release_dopamine(args->sys, 1.0f, 0.5f);
                }
            }
        }

        return nullptr;
    };

    /* WHAT: Most threads are monitors (6), few are updaters (2)
     * WHY:  Typical monitoring scenario - many readers, few writers
     */
    std::vector<ThreadArgs> thread_args(NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        thread_args[i] = {
            system,
            &ready_count,
            &start,
            &stop,
            &stats_read_count,
            i < 6  // 6 monitors, 2 learners
        };
        pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
    }

    while (ready_count.load(std::memory_order_acquire) < NUM_THREADS) {
        // Spin wait
    }
    start.store(true, std::memory_order_release);

    /* WHAT: Let run for 100ms then stop
     * WHY:  Sufficient time for many concurrent reads
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], nullptr);
    }

    /* WHAT: Verify many stats reads succeeded
     * WHY:  Reader-writer lock should allow parallel monitoring
     * EXPECT: Thousands of reads (readers don't block each other)
     */
    uint32_t total_reads = stats_read_count.load();
    EXPECT_GT(total_reads, 1000);  // Should get many concurrent reads

    std::cout << "Total stats reads during learning: " << total_reads << std::endl;
}

//=============================================================================
// Performance Benchmarks
//=============================================================================

/**
 * @brief Benchmark neuromodulator update throughput
 *
 * WHAT: Measure updates per second with varying thread counts
 * WHY:  Verify scalability of locking strategy
 */
TEST_F(NeuromodulatorThreadSafetyTest, DISABLED_BenchmarkUpdateThroughput) {
    /* WHAT: Measure throughput for 1, 2, 4, 8 threads
     * WHY:  Identify lock contention bottlenecks
     * NOTE: Disabled by default (run manually for benchmarking)
     */

    for (uint32_t num_threads : {1u, 2u, 4u, 8u}) {
        std::atomic<uint32_t> ready_count{0};
        std::atomic<bool> start{false};
        std::vector<pthread_t> threads(num_threads);

        struct ThreadArgs {
            neuromodulator_system_t sys;
            std::atomic<uint32_t>* ready;
            std::atomic<bool>* start;
            uint32_t ops;
        };

        auto thread_func = +[](void* arg) -> void* {
            ThreadArgs* args = static_cast<ThreadArgs*>(arg);

            args->ready->fetch_add(1, std::memory_order_release);
            while (!args->start->load(std::memory_order_acquire)) {
                // Spin wait
            }

            for (uint32_t i = 0; i < args->ops; i++) {
                neuromodulator_update(args->sys, 0.001f);
            }

            return nullptr;
        };

        std::vector<ThreadArgs> thread_args(num_threads);
        for (uint32_t i = 0; i < num_threads; i++) {
            thread_args[i] = {system, &ready_count, &start, OPS_PER_THREAD};
            pthread_create(&threads[i], nullptr, thread_func, &thread_args[i]);
        }

        while (ready_count.load(std::memory_order_acquire) < num_threads) {
            // Spin wait
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        start.store(true, std::memory_order_release);

        for (uint32_t i = 0; i < num_threads; i++) {
            pthread_join(threads[i], nullptr);
        }
        auto end_time = std::chrono::high_resolution_clock::now();

        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        uint64_t total_ops = num_threads * OPS_PER_THREAD;
        double ops_per_sec = (total_ops * 1000.0) / duration_ms;

        std::cout << "Threads: " << num_threads
                  << ", Ops/sec: " << ops_per_sec
                  << ", Duration: " << duration_ms << " ms" << std::endl;
    }
}
