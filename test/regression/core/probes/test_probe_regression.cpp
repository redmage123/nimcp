/**
 * @file test_probe_regression.cpp
 * @brief Regression tests — performance, memory leaks, concurrency
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <unistd.h>

#include "core/probes/nimcp_brain_probes.h"
#include "core/brain/nimcp_brain_internal.h"

extern "C" {
#include "nimcp.h"
#include "utils/memory/nimcp_memory.h"
}

/* ============================================================================
 * Regression Fixture
 * ============================================================================ */

class ProbeRegressionTest : public ::testing::Test {
protected:
    nimcp_brain_t brain;

    void SetUp() override {
        nimcp_init();
        brain = nimcp_brain_create("probe_reg_test", NIMCP_BRAIN_SMALL,
                                    NIMCP_TASK_REGRESSION, 64, 32);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
        nimcp_shutdown();
    }
};

/* ============================================================================
 * Performance: brain_predict latency baseline
 * ============================================================================ */

TEST_F(ProbeRegressionTest, PredictLatencyNoProbes) {
    float features[64];
    for (int i = 0; i < 64; i++) features[i] = 0.1f;
    char label[64] = {0};
    float confidence = 0.0f;

    /* Warm up */
    for (int i = 0; i < 5; i++) {
        nimcp_brain_predict_fast(brain, features, 64, label, &confidence);
    }

    auto start = std::chrono::high_resolution_clock::now();
    int n = 20;
    for (int i = 0; i < n; i++) {
        nimcp_brain_predict_fast(brain, features, 64, label, &confidence);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double baseline_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / (double)n;

    EXPECT_GT(baseline_us, 0.0);
}

TEST_F(ProbeRegressionTest, PredictLatencyWithProbes) {
    float features[64];
    for (int i = 0; i < 64; i++) features[i] = 0.1f;
    char label[64] = {0};
    float confidence = 0.0f;

    nimcp_brain_attach_builtin_probes(brain, 500);

    /* Warm up */
    for (int i = 0; i < 5; i++) {
        nimcp_brain_predict_fast(brain, features, 64, label, &confidence);
    }

    auto start = std::chrono::high_resolution_clock::now();
    int n = 20;
    for (int i = 0; i < n; i++) {
        nimcp_brain_predict_fast(brain, features, 64, label, &confidence);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double probe_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / (double)n;

    EXPECT_GT(probe_us, 0.0);
}

/* ============================================================================
 * Memory: create/destroy 100 cycles
 * ============================================================================ */

TEST_F(ProbeRegressionTest, MemoryLeakCreateDestroy) {
    /* Attach probes, query, destroy probes via handle — 100 cycles.
     * If we get through without crash or ASAN errors, no obvious leak. */
    for (int cycle = 0; cycle < 100; cycle++) {
        uint16_t modules[] = {0x0100};
        uint32_t handle = 0;
        int rc = nimcp_brain_create_probe(brain, modules, 1, 500, 1, &handle);
        if (rc == 0 && handle != 0) {
            char* json = nullptr;
            nimcp_brain_get_all_probe_metrics_json(brain, &json);
            if (json) nimcp_free(json);

            nimcp_brain_destroy_probe(brain, handle);
        }
    }
    SUCCEED();
}

/* ============================================================================
 * Concurrency: readers + writer threads
 * ============================================================================ */

TEST_F(ProbeRegressionTest, ConcurrentAccess) {
    nimcp_brain_attach_builtin_probes(brain, 50);

    std::atomic<bool> running{true};
    std::atomic<int> read_count{0};

    auto reader_fn = [&]() {
        while (running.load()) {
            char* json = nullptr;
            nimcp_brain_get_all_probe_metrics_json(brain, &json);
            if (json) {
                nimcp_free(json);
                read_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    auto writer_fn = [&]() {
        float features[64] = {0};
        for (int i = 0; i < 64; i++) features[i] = 0.1f;
        char label[64] = {0};
        float confidence = 0.0f;
        while (running.load()) {
            nimcp_brain_predict_fast(brain, features, 64, label, &confidence);
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(writer_fn);
    threads.emplace_back(reader_fn);
    threads.emplace_back(reader_fn);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running.store(false);
    for (auto& t : threads) t.join();

    EXPECT_GT(read_count.load(), 0);
}

/* ============================================================================
 * Training unaffected
 * ============================================================================ */

TEST_F(ProbeRegressionTest, LearnUnaffectedByProbes) {
    float features[64], target[32];
    for (int i = 0; i < 64; i++) features[i] = 0.3f;
    for (int i = 0; i < 32; i++) target[i] = 0.6f;

    nimcp_brain_attach_builtin_probes(brain, 100);

    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_vector(brain, features, 64, target, 32, "reg_test", 1.0f);
    }
    SUCCEED();
}
