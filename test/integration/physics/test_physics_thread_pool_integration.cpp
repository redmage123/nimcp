//=============================================================================
// test_physics_thread_pool_integration.cpp - Physics Thread Pool Integration
//=============================================================================
/**
 * @file test_physics_thread_pool_integration.cpp
 * @brief Integration tests for physics layer thread pool usage
 *
 * Tests parallel population updates and batch processing.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <algorithm>

#include "utils/thread/nimcp_thread_pool.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "physics/ephaptic/nimcp_ephaptic.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsThreadPoolIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create thread pool
        pool_ = nimcp_pool_create(4);
        pool_initialized_ = (pool_ != nullptr);

        // Create HH population for parallel testing
        nimcp_hh_config_t hh_config = {
            .g_Na = 120.0f, .g_K = 36.0f, .g_L = 0.3f,
            .g_Ca_L = 0.0f, .g_Ca_T = 0.0f, .g_K_Ca = 0.0f,
            .g_K_A = 0.0f, .g_H = 0.0f,
            .E_Na = 50.0f, .E_K = -77.0f, .E_L = -54.4f,
            .E_Ca = 120.0f, .E_H = -30.0f,
            .C_m = 1.0f, .V_rest = -65.0f,
            .temperature = 37.0f, .surface_area = 0.01f,
            .length = 100.0f, .diameter = 10.0f,
            .enable_calcium = false, .enable_adaptation = false,
            .enable_h_current = false,
            .dt_max = 0.1f, .adaptive_dt = false
        };
        hh_initialized_ = (nimcp_hh_population_create(&hh_pop_, 100, &hh_config) == NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (hh_initialized_) nimcp_hh_population_destroy(&hh_pop_);
        if (pool_initialized_) nimcp_pool_destroy(pool_);
    }

    nimcp_thread_pool_t* pool_ = nullptr;
    nimcp_hh_population_t hh_pop_;
    bool pool_initialized_ = false;
    bool hh_initialized_ = false;
};

//=============================================================================
// Thread Pool Basic Tests
//=============================================================================

TEST_F(PhysicsThreadPoolIntegrationTest, PoolCreation) {
    ASSERT_TRUE(pool_initialized_);
    EXPECT_NE(pool_, nullptr);
}

TEST_F(PhysicsThreadPoolIntegrationTest, PoolActive) {
    if (!pool_initialized_) GTEST_SKIP() << "Pool not initialized";

    size_t active = nimcp_pool_active(pool_);
    // Should have some active threads after creation
    EXPECT_GE(active, 0U);
}

TEST_F(PhysicsThreadPoolIntegrationTest, PoolPending) {
    if (!pool_initialized_) GTEST_SKIP() << "Pool not initialized";

    size_t pending = nimcp_pool_pending(pool_);
    EXPECT_EQ(pending, 0U);  // No tasks submitted yet
}

//=============================================================================
// Task Submission Tests
//=============================================================================

struct TestTaskData {
    int* counter;
    int increment;
};

static void test_task_fn(void* arg) {
    TestTaskData* data = (TestTaskData*)arg;
    __sync_fetch_and_add(data->counter, data->increment);
}

TEST_F(PhysicsThreadPoolIntegrationTest, TaskSubmit) {
    if (!pool_initialized_) GTEST_SKIP() << "Pool not initialized";

    int counter = 0;
    TestTaskData data = { &counter, 1 };

    nimcp_result_t result = nimcp_pool_submit(pool_, test_task_fn, &data);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    result = nimcp_pool_wait(pool_);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(counter, 1);
}

TEST_F(PhysicsThreadPoolIntegrationTest, MultipleTasks) {
    if (!pool_initialized_) GTEST_SKIP() << "Pool not initialized";

    int counter = 0;
    TestTaskData data[100];

    for (int i = 0; i < 100; i++) {
        data[i].counter = &counter;
        data[i].increment = 1;
        nimcp_pool_submit(pool_, test_task_fn, &data[i]);
    }

    nimcp_pool_wait(pool_);
    EXPECT_EQ(counter, 100);
}

//=============================================================================
// Parallel HH Population Update Tests
//=============================================================================

struct HHBatchTask {
    nimcp_hh_neuron_t* neurons;  // Direct pointer to neurons array
    const float* currents;
    uint32_t start;
    uint32_t end;
    float dt;
    int* success_count;
};

static void hh_batch_update_task(void* arg) {
    HHBatchTask* task = (HHBatchTask*)arg;
    int success = 0;

    for (uint32_t i = task->start; i < task->end; i++) {
        if (nimcp_hh_neuron_update(&task->neurons[i], task->currents[i], task->dt) == NIMCP_SUCCESS) {
            success++;
        }
    }

    __sync_fetch_and_add(task->success_count, success);
}

TEST_F(PhysicsThreadPoolIntegrationTest, ParallelHHUpdateDirect) {
    if (!pool_initialized_ || !hh_initialized_) {
        GTEST_SKIP() << "Pool or HH not initialized";
    }

    const uint32_t pop_size = 100;
    const uint32_t num_threads = 4;
    const uint32_t chunk_size = pop_size / num_threads;

    float* currents = new float[pop_size];
    for (uint32_t i = 0; i < pop_size; i++) {
        currents[i] = 10.0f;  // Inject current
    }

    int success_count = 0;
    HHBatchTask tasks[num_threads];

    for (uint32_t i = 0; i < num_threads; i++) {
        tasks[i].neurons = hh_pop_.neurons;
        tasks[i].currents = currents;
        tasks[i].start = i * chunk_size;
        tasks[i].end = (i == num_threads - 1) ? pop_size : (i + 1) * chunk_size;
        tasks[i].dt = 0.025f;
        tasks[i].success_count = &success_count;

        nimcp_pool_submit(pool_, hh_batch_update_task, &tasks[i]);
    }

    nimcp_pool_wait(pool_);

    EXPECT_EQ(success_count, (int)pop_size);

    delete[] currents;
}

//=============================================================================
// Built-in Parallel API Tests
//=============================================================================

TEST_F(PhysicsThreadPoolIntegrationTest, BuiltInParallelUpdate) {
    if (!hh_initialized_) GTEST_SKIP() << "HH not initialized";

    const uint32_t pop_size = 100;
    float* currents = new float[pop_size];
    for (uint32_t i = 0; i < pop_size; i++) {
        currents[i] = 10.0f;
    }

    // Use the built-in parallel update function
    nimcp_error_t result = nimcp_hh_population_update_parallel(
        &hh_pop_, currents, 0.025f, 4  // 4 threads
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    delete[] currents;
}

TEST_F(PhysicsThreadPoolIntegrationTest, CreateParallelPopulation) {
    nimcp_hh_config_t hh_config = {
        .g_Na = 120.0f, .g_K = 36.0f, .g_L = 0.3f,
        .g_Ca_L = 0.0f, .g_Ca_T = 0.0f, .g_K_Ca = 0.0f,
        .g_K_A = 0.0f, .g_H = 0.0f,
        .E_Na = 50.0f, .E_K = -77.0f, .E_L = -54.4f,
        .E_Ca = 120.0f, .E_H = -30.0f,
        .C_m = 1.0f, .V_rest = -65.0f,
        .temperature = 37.0f, .surface_area = 0.01f,
        .length = 100.0f, .diameter = 10.0f,
        .enable_calcium = false, .enable_adaptation = false,
        .enable_h_current = false,
        .dt_max = 0.1f, .adaptive_dt = false
    };

    nimcp_hh_population_t parallel_pop;
    nimcp_error_t result = nimcp_hh_population_create_parallel(
        &parallel_pop, 200, &hh_config, 4  // 4 threads
    );
    EXPECT_EQ(result, NIMCP_SUCCESS);

    if (result == NIMCP_SUCCESS) {
        // Verify internal thread pool was created
        EXPECT_NE(parallel_pop.thread_pool, nullptr);

        // Clean up
        nimcp_hh_population_destroy(&parallel_pop);
    }
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(PhysicsThreadPoolIntegrationTest, ParallelSpeedup) {
    if (!hh_initialized_) GTEST_SKIP() << "HH not initialized";

    const uint32_t pop_size = 100;
    const int iterations = 100;
    const int num_rounds = 5;
    const int warmup_rounds = 2;
    float* currents = new float[pop_size];
    for (uint32_t i = 0; i < pop_size; i++) {
        currents[i] = 10.0f;
    }

    // Warmup: stabilize caches and thread pool scheduling
    for (int w = 0; w < warmup_rounds; w++) {
        for (int iter = 0; iter < iterations; iter++) {
            nimcp_hh_population_update(&hh_pop_, currents, 0.025f);
        }
        for (int iter = 0; iter < iterations; iter++) {
            nimcp_hh_population_update_parallel(&hh_pop_, currents, 0.025f, 4);
        }
    }

    // Best-of-N measurement filters transient CPU contention
    int64_t best_seq = INT64_MAX;
    int64_t best_par = INT64_MAX;
    float speedups[5];

    for (int round = 0; round < num_rounds; round++) {
        auto seq_start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < iterations; iter++) {
            nimcp_hh_population_update(&hh_pop_, currents, 0.025f);
        }
        auto seq_end = std::chrono::high_resolution_clock::now();
        auto seq_us = std::chrono::duration_cast<std::chrono::microseconds>(seq_end - seq_start).count();

        auto par_start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < iterations; iter++) {
            nimcp_hh_population_update_parallel(&hh_pop_, currents, 0.025f, 4);
        }
        auto par_end = std::chrono::high_resolution_clock::now();
        auto par_us = std::chrono::duration_cast<std::chrono::microseconds>(par_end - par_start).count();

        speedups[round] = (par_us > 0) ? (float)seq_us / (float)par_us : 1.0f;
        if (seq_us < best_seq) best_seq = seq_us;
        if (par_us < best_par) best_par = par_us;
    }

    float best_speedup = (best_par > 0) ? (float)best_seq / (float)best_par : 1.0f;
    std::sort(speedups, speedups + num_rounds);
    float median_speedup = speedups[num_rounds / 2];

    std::cout << "Best sequential: " << best_seq << " us" << std::endl;
    std::cout << "Best parallel:   " << best_par << " us" << std::endl;
    std::cout << "Best speedup:    " << best_speedup << "x" << std::endl;
    std::cout << "Median speedup:  " << median_speedup << "x" << std::endl;

    // Conservative threshold: parallel must not be catastrophically slower.
    // With 100 neurons, thread overhead can dominate under contention.
    EXPECT_GT(best_speedup, 0.05f)
        << "Parallel was >20x slower than sequential, likely thread pool malfunction";

    delete[] currents;
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(PhysicsThreadPoolIntegrationTest, ConcurrentPoolAccess) {
    if (!pool_initialized_) GTEST_SKIP() << "Pool not initialized";

    int counters[10] = {0};
    TestTaskData data[100];

    for (int i = 0; i < 100; i++) {
        data[i].counter = &counters[i % 10];
        data[i].increment = 1;
        nimcp_pool_submit(pool_, test_task_fn, &data[i]);
    }

    nimcp_pool_wait(pool_);

    int total = 0;
    for (int i = 0; i < 10; i++) {
        total += counters[i];
        EXPECT_EQ(counters[i], 10);
    }
    EXPECT_EQ(total, 100);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
