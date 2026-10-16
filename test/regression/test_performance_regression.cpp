/**
 * @file test_performance_regression.cpp
 * @brief Regression tests for performance and memory usage
 *
 * WHAT: Performance and memory regression tests for implemented features
 * WHY:  Ensure no performance degradation over time
 * HOW:  Benchmark operations, track memory usage, compare against baselines
 *
 * @author NIMCP Test Team
 * @date 2025-01-17
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "utils/spatial/nimcp_kdtree.h"
#include "utils/config/nimcp_dynamic_config.h"
#include <array>
#include <chrono>
#include <vector>
#include <random>
#include <sys/resource.h>

//=============================================================================
// Performance Utilities
//=============================================================================

class PerformanceMonitor {
public:
    static long GetMemoryUsageKB() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss;  // KB on Linux, bytes on macOS
    }

    template<typename Func>
    static double MeasureTimeMs(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

//=============================================================================
// KD-Tree Performance Regression
//=============================================================================

TEST(KDTreePerformanceRegression, BuildTime_1000Points) {
    // WHAT: Measure KD-tree build time
    // WHY:  Ensure build time stays within bounds
    // HOW:  Build tree with 1000 points, measure time

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    std::vector<std::array<float, 3>> points;
    std::vector<void*> user_data;

    for (int i = 0; i < 1000; i++) {
        std::array<float, 3> pt = {{dist(rng), dist(rng), dist(rng)}};
        points.push_back(pt);
        user_data.push_back((void*)(uintptr_t)i);
    }

    kdtree_t* tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    double build_time = PerformanceMonitor::MeasureTimeMs([&]() {
        kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(points.data()), user_data.data(), 1000);
    });

    kdtree_destroy(tree);

    std::cout << "KD-tree build (1000 points): " << build_time << " ms" << std::endl;

    // Regression baseline: < 5ms for 1000 points
    EXPECT_LT(build_time, 5.0);
}

TEST(KDTreePerformanceRegression, QueryTime_RangeSearch) {
    // WHAT: Measure range search query time
    // WHY:  Ensure queries stay fast
    // HOW:  1000 points, 100 queries, measure average time

    std::mt19937 rng(123);
    std::uniform_real_distribution<float> dist(-50.0f, 50.0f);

    std::vector<std::array<float, 3>> points;
    std::vector<void*> user_data;

    for (int i = 0; i < 1000; i++) {
        std::array<float, 3> pt = {{dist(rng), dist(rng), dist(rng)}};
        points.push_back(pt);
        user_data.push_back((void*)(uintptr_t)i);
    }

    kdtree_t* tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(points.data()), user_data.data(), 1000);

    void* results[100];
    double total_query_time = 0.0;

    for (int i = 0; i < 100; i++) {
        std::array<float, 3> query = {{dist(rng), dist(rng), dist(rng)}};

        double query_time = PerformanceMonitor::MeasureTimeMs([&]() {
            kdtree_range_search(tree, query.data(), 10.0f, results, 100);
        });

        total_query_time += query_time;
    }

    kdtree_destroy(tree);

    double avg_query_time = total_query_time / 100.0;

    std::cout << "KD-tree query (avg of 100): " << avg_query_time << " ms" << std::endl;

    // Regression baseline: < 0.1ms average per query
    EXPECT_LT(avg_query_time, 0.1);
}

TEST(KDTreePerformanceRegression, MemoryUsage_10000Points) {
    // WHAT: Measure memory usage for large tree
    // WHY:  Ensure no memory leaks or bloat
    // HOW:  Track memory before/after building tree

    long mem_before = PerformanceMonitor::GetMemoryUsageKB();

    std::mt19937 rng(456);
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);

    std::vector<std::array<float, 3>> points;
    std::vector<void*> user_data;

    for (int i = 0; i < 10000; i++) {
        std::array<float, 3> pt = {{dist(rng), dist(rng), dist(rng)}};
        points.push_back(pt);
        user_data.push_back((void*)(uintptr_t)i);
    }

    kdtree_t* tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(points.data()), user_data.data(), 10000);

    long mem_after = PerformanceMonitor::GetMemoryUsageKB();

    kdtree_destroy(tree);

    long mem_used = mem_after - mem_before;

    std::cout << "KD-tree memory (10k points): " << mem_used << " KB" << std::endl;

    // Regression baseline: < 15MB for 10k points (realistic for tree structure overhead)
    EXPECT_LT(mem_used, 15000);
}

//=============================================================================
// Config Callbacks Performance Regression
//=============================================================================

TEST(ConfigCallbacksPerformanceRegression, RegistrationTime) {
    // WHAT: Measure callback registration time
    // WHY:  Ensure registration is fast
    // HOW:  Register 50 callbacks, measure time

    system("echo 'test=1' > /tmp/perf_config.ini");
    config_init("/tmp/perf_config.ini");

    std::vector<uint32_t> ids;

    auto dummy_callback = [](const char* key, const config_value_t* old_val,
                            const config_value_t* new_val, void* user_data) {
        (void)key; (void)old_val; (void)new_val; (void)user_data;
    };

    double reg_time = PerformanceMonitor::MeasureTimeMs([&]() {
        for (int i = 0; i < 50; i++) {
            uint32_t id = config_register_callback("test", dummy_callback, nullptr);
            if (id != 0) {
                ids.push_back(id);
            }
        }
    });

    for (uint32_t id : ids) {
        config_unregister_callback(id);
    }

    config_shutdown();
    system("rm -f /tmp/perf_config.ini");

    std::cout << "Config callback registration (50 callbacks): " << reg_time << " ms" << std::endl;

    // Regression baseline: < 1ms for 50 registrations
    EXPECT_LT(reg_time, 1.0);
}

TEST(ConfigCallbacksPerformanceRegression, InvocationTime) {
    // WHAT: Measure callback invocation overhead
    // WHY:  Ensure config changes are fast even with many callbacks
    // HOW:  50 callbacks, measure config change time

    system("echo 'test=1' > /tmp/perf_config.ini");
    config_init("/tmp/perf_config.ini");

    std::vector<uint32_t> ids;
    std::atomic<int> counter{0};

    auto counter_callback = [](const char* key, const config_value_t* old_val,
                              const config_value_t* new_val, void* user_data) {
        (void)key; (void)old_val; (void)new_val;
        auto* cnt = static_cast<std::atomic<int>*>(user_data);
        (*cnt)++;
    };

    // Register 50 callbacks
    for (int i = 0; i < 50; i++) {
        uint32_t id = config_register_callback("test", counter_callback, &counter);
        if (id != 0) {
            ids.push_back(id);
        }
    }

    double invoke_time = PerformanceMonitor::MeasureTimeMs([&]() {
        config_set_int("test", 42);
    });

    for (uint32_t id : ids) {
        config_unregister_callback(id);
    }

    config_shutdown();
    system("rm -f /tmp/perf_config.ini");

    std::cout << "Config callback invocation (50 callbacks): " << invoke_time << " ms" << std::endl;

    // All 50 callbacks should have been invoked
    EXPECT_EQ(counter, 50);

    // Regression baseline: < 1ms for invoking 50 callbacks
    EXPECT_LT(invoke_time, 1.0);
}

//=============================================================================
// Layer Freezing Performance Regression
//=============================================================================

TEST(LayerFreezingPerformanceRegression, FinetuneTime_SmallBrain) {
    // WHAT: Measure fine-tuning time with layer freezing
    // WHY:  Ensure no significant overhead from freezing logic
    // HOW:  Train TINY brain for 5 epochs, measure time

    brain_t brain = brain_create("perf_brain", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr);

    // Generate training data
    std::vector<float> training_data;
    std::vector<float> labels;

    for (int i = 0; i < 50; i++) {
        for (int j = 0; j < 10; j++) {
            training_data.push_back((float)(i * 10 + j) / 500.0f);
        }
        for (int j = 0; j < 10; j++) {
            labels.push_back((j == (i % 10)) ? 1.0f : 0.0f);
        }
    }

    brain_finetune_config_t config = {
        .learning_rate = 0.01f,
        .num_epochs = 5,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = 16,
        .verbose = false
    };

    double finetune_time = PerformanceMonitor::MeasureTimeMs([&]() {
        brain_finetune(brain, training_data.data(), labels.data(), 50, &config);
    });

    brain_destroy(brain);

    std::cout << "Layer freezing fine-tune (50 samples, 5 epochs): "
              << finetune_time << " ms" << std::endl;

    // Regression baseline: < 500ms for small brain
    EXPECT_LT(finetune_time, 500.0);
}

TEST(LayerFreezingPerformanceRegression, MemoryUsage_Finetune) {
    // WHAT: Measure memory during fine-tuning
    // WHY:  Ensure no memory leaks during training
    // HOW:  Track memory before/after fine-tuning

    long mem_before = PerformanceMonitor::GetMemoryUsageKB();

    brain_t brain = brain_create("mem_brain", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 10, 10);
    ASSERT_NE(brain, nullptr);

    std::vector<float> training_data;
    std::vector<float> labels;

    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 10; j++) {
            training_data.push_back((float)(i + j) / 100.0f);
        }
        for (int j = 0; j < 10; j++) {
            labels.push_back((j == (i % 10)) ? 1.0f : 0.0f);
        }
    }

    brain_finetune_config_t config = {
        .learning_rate = 0.01f,
        .num_epochs = 3,
        .freeze_sensory = false,
        .freeze_cognitive = false,
        .finetune_classifier = true,
        .batch_size = 32,
        .verbose = false
    };

    brain_finetune(brain, training_data.data(), labels.data(), 100, &config);

    long mem_after = PerformanceMonitor::GetMemoryUsageKB();

    brain_destroy(brain);

    long mem_used = mem_after - mem_before;

    std::cout << "Fine-tuning memory overhead: " << mem_used << " KB" << std::endl;

    // Regression baseline: < 100MB overhead
    EXPECT_LT(mem_used, 100000);
}

//=============================================================================
// Combined Integration Performance
//=============================================================================

TEST(PerformanceRegression, EndToEnd_AllFeatures) {
    // WHAT: End-to-end performance with all features
    // WHY:  Ensure combined usage is performant
    // HOW:  Use KD-tree + callbacks + layer freezing together

    system("echo 'lr=0.01' > /tmp/e2e_config.ini");
    config_init("/tmp/e2e_config.ini");

    brain_t brain = brain_create("e2e_brain", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION, 128, 20);
    ASSERT_NE(brain, nullptr);

    // KD-tree for spatial indexing
    kdtree_t* tree = kdtree_create();
    ASSERT_NE(tree, nullptr);

    std::mt19937 rng(789);
    std::uniform_real_distribution<float> dist(-5.0f, 5.0f);

    std::vector<std::array<float, 3>> points;
    std::vector<void*> user_data;

    for (int i = 0; i < 500; i++) {
        std::array<float, 3> pt = {{dist(rng), dist(rng), dist(rng)}};
        points.push_back(pt);
        user_data.push_back((void*)(uintptr_t)i);
    }

    kdtree_build(tree, reinterpret_cast<const kdtree_point_t*>(points.data()), user_data.data(), 500);

    // Config callback
    auto callback = [](const char* k, const config_value_t* o,
                      const config_value_t* n, void* u) {
        (void)k; (void)o; (void)n; (void)u;
    };

    uint32_t cb_id = config_register_callback("lr", callback, nullptr);

    // Training data - brain has 128 inputs, 20 outputs, 30 samples
    std::vector<float> training_data(30 * 128);  // 30 samples × 128 inputs
    std::vector<float> labels(30 * 20);          // 30 samples × 20 outputs

    brain_finetune_config_t ft_config = {
        .learning_rate = 0.01f,
        .num_epochs = 2,
        .freeze_sensory = true,
        .freeze_cognitive = true,
        .finetune_classifier = true,
        .batch_size = 16,
        .verbose = false
    };

    double total_time = PerformanceMonitor::MeasureTimeMs([&]() {
        // KD-tree query
        std::array<float, 3> query = {{0.0f, 0.0f, 0.0f}};
        void* results[50];
        kdtree_range_search(tree, query.data(), 2.0f, results, 50);

        // Config change
        config_set_float("lr", 0.005);

        // Brain training
        brain_finetune(brain, training_data.data(), labels.data(), 30, &ft_config);
    });

    config_unregister_callback(cb_id);
    kdtree_destroy(tree);
    brain_destroy(brain);
    config_shutdown();
    system("rm -f /tmp/e2e_config.ini");

    std::cout << "End-to-end combined operations: " << total_time << " ms" << std::endl;

    // Regression baseline: < 1000ms for combined operations
    EXPECT_LT(total_time, 1000.0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
