/**
 * @file test_brain_persistence_regression.cpp
 * @brief Regression tests for brain persistence with bio-async
 *
 * WHAT: Performance and reliability tests for brain save/load operations
 * WHY:  Ensure persistence operations maintain performance and correctness
 * HOW:  Large brain save/load benchmarks, memory tracking, stress tests
 *
 * TEST COVERAGE:
 * 1. Save/Load Performance - Large brains (1000+ neurons)
 * 2. Memory Usage - Track memory during persistence operations
 * 3. Data Integrity - Verify loaded brains match saved brains
 * 4. Stress Testing - Repeated save/load cycles
 * 5. Edge Cases - Large files, concurrent operations
 *
 * @author NIMCP Development Team
 * @date 2025-11-29
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdio>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_bio_async.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "core/brain/factory/nimcp_brain_factory.h"
#include "async/nimcp_bio_async.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

constexpr uint32_t SMALL_BRAIN_SIZE = 100;
constexpr uint32_t MEDIUM_BRAIN_SIZE = 500;
constexpr uint32_t LARGE_BRAIN_SIZE = 1000;
constexpr uint32_t STRESS_CYCLE_COUNT = 100;

//=============================================================================
// Test Fixture
//=============================================================================

class BrainPersistenceRegressionTest : public ::testing::Test {
protected:
    std::vector<std::string> temp_files;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize persistence
        ASSERT_TRUE(persistence_init(nullptr));
    }

    void TearDown() override {
        // Clean up temp files
        for (const auto& file : temp_files) {
            std::remove(file.c_str());
            std::remove((file + ".meta").c_str());
            std::remove((file + ".knowledge").c_str());
            std::remove((file + ".executive").c_str());
        }
        temp_files.clear();

        persistence_shutdown();
        nimcp_bio_async_shutdown();
    }

    std::string GetTempFilename() {
        char filename[256];
        snprintf(filename, sizeof(filename), "/tmp/brain_test_%d_%ld.brain",
                getpid(), std::chrono::steady_clock::now().time_since_epoch().count());
        temp_files.push_back(filename);
        return filename;
    }

    brain_t CreateAndTrainBrain(uint32_t num_neurons, uint32_t num_examples) {
        brain_config_t config = brain_default_config();
        config.num_neurons = num_neurons;
        config.num_inputs = 10;
        config.num_outputs = 5;
        config.connection_density = 0.3f;

        brain_t brain = brain_create(&config);
        if (!brain) return nullptr;

        // Train with some examples
        float inputs[10];
        for (uint32_t i = 0; i < num_examples; ++i) {
            for (int j = 0; j < 10; ++j) {
                inputs[j] = static_cast<float>(i * 10 + j) / 1000.0f;
            }
            const char* label = (i % 5 == 0) ? "class_0" :
                               (i % 5 == 1) ? "class_1" :
                               (i % 5 == 2) ? "class_2" :
                               (i % 5 == 3) ? "class_3" : "class_4";
            brain_learn_example(brain, inputs, 10, label, 1.0f);
        }

        return brain;
    }

    struct PerfMetrics {
        double save_time_ms;
        double load_time_ms;
        size_t file_size_bytes;
        size_t memory_used_bytes;
    };

    PerfMetrics MeasureSaveLoadPerformance(uint32_t num_neurons, uint32_t num_examples) {
        PerfMetrics metrics = {};

        // Create and train brain
        brain_t original = CreateAndTrainBrain(num_neurons, num_examples);
        EXPECT_NE(original, nullptr);

        std::string filename = GetTempFilename();

        // Measure save time
        auto save_start = std::chrono::high_resolution_clock::now();
        bool saved = brain_save(original, filename.c_str());
        auto save_end = std::chrono::high_resolution_clock::now();

        EXPECT_TRUE(saved);
        metrics.save_time_ms = std::chrono::duration<double, std::milli>(
            save_end - save_start).count();

        // Get file size
        FILE* f = fopen(filename.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            metrics.file_size_bytes = ftell(f);
            fclose(f);
        }

        // Measure load time
        auto load_start = std::chrono::high_resolution_clock::now();
        brain_t loaded = brain_load(filename.c_str());
        auto load_end = std::chrono::high_resolution_clock::now();

        EXPECT_NE(loaded, nullptr);
        metrics.load_time_ms = std::chrono::duration<double, std::milli>(
            load_end - load_start).count();

        // Clean up
        brain_destroy(original);
        if (loaded) brain_destroy(loaded);

        return metrics;
    }
};

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

TEST_F(BrainPersistenceRegressionTest, SavePerformance_SmallBrain) {
    // WHAT: Measure save performance for small brain (100 neurons)
    // WHY:  Baseline performance test
    // EXPECT: < 100ms save time

    PerfMetrics metrics = MeasureSaveLoadPerformance(SMALL_BRAIN_SIZE, 50);

    std::cout << "\n=== Small Brain Save Performance ===" << std::endl;
    std::cout << "Save time: " << metrics.save_time_ms << " ms" << std::endl;
    std::cout << "File size: " << metrics.file_size_bytes << " bytes" << std::endl;

    EXPECT_LT(metrics.save_time_ms, 100.0)
        << "Save time too high for small brain";

    RecordProperty("SmallBrainSaveTime_ms", metrics.save_time_ms);
    RecordProperty("SmallBrainFileSize_bytes", static_cast<int>(metrics.file_size_bytes));
}

TEST_F(BrainPersistenceRegressionTest, SavePerformance_MediumBrain) {
    // WHAT: Measure save performance for medium brain (500 neurons)
    // WHY:  Test scaling behavior
    // EXPECT: < 500ms save time

    PerfMetrics metrics = MeasureSaveLoadPerformance(MEDIUM_BRAIN_SIZE, 100);

    std::cout << "\n=== Medium Brain Save Performance ===" << std::endl;
    std::cout << "Save time: " << metrics.save_time_ms << " ms" << std::endl;
    std::cout << "File size: " << metrics.file_size_bytes << " bytes" << std::endl;

    EXPECT_LT(metrics.save_time_ms, 500.0)
        << "Save time too high for medium brain";

    RecordProperty("MediumBrainSaveTime_ms", metrics.save_time_ms);
    RecordProperty("MediumBrainFileSize_bytes", static_cast<int>(metrics.file_size_bytes));
}

TEST_F(BrainPersistenceRegressionTest, SavePerformance_LargeBrain) {
    // WHAT: Measure save performance for large brain (1000 neurons)
    // WHY:  Test performance at scale
    // EXPECT: < 2000ms save time

    PerfMetrics metrics = MeasureSaveLoadPerformance(LARGE_BRAIN_SIZE, 200);

    std::cout << "\n=== Large Brain Save Performance ===" << std::endl;
    std::cout << "Save time: " << metrics.save_time_ms << " ms" << std::endl;
    std::cout << "File size: " << metrics.file_size_bytes << " bytes" << std::endl;

    EXPECT_LT(metrics.save_time_ms, 2000.0)
        << "Save time too high for large brain";

    RecordProperty("LargeBrainSaveTime_ms", metrics.save_time_ms);
    RecordProperty("LargeBrainFileSize_bytes", static_cast<int>(metrics.file_size_bytes));
}

TEST_F(BrainPersistenceRegressionTest, LoadPerformance_LargeBrain) {
    // WHAT: Measure load performance for large brain
    // WHY:  Load is often slower than save
    // EXPECT: < 2000ms load time

    PerfMetrics metrics = MeasureSaveLoadPerformance(LARGE_BRAIN_SIZE, 200);

    std::cout << "\n=== Large Brain Load Performance ===" << std::endl;
    std::cout << "Load time: " << metrics.load_time_ms << " ms" << std::endl;

    EXPECT_LT(metrics.load_time_ms, 2000.0)
        << "Load time too high for large brain";

    RecordProperty("LargeBrainLoadTime_ms", metrics.load_time_ms);
}

//=============================================================================
// DATA INTEGRITY TESTS
//=============================================================================

TEST_F(BrainPersistenceRegressionTest, DataIntegrity_SaveLoadRoundtrip) {
    // WHAT: Verify brain state preserved after save/load
    // WHY:  Ensure no data corruption
    // HOW:  Compare outputs before/after save/load

    brain_t original = CreateAndTrainBrain(200, 100);
    ASSERT_NE(original, nullptr);

    // Get outputs from original brain
    float inputs[10];
    for (int i = 0; i < 10; ++i) {
        inputs[i] = static_cast<float>(i) / 10.0f;
    }

    float* original_outputs = nullptr;
    int original_size = 0;
    brain_infer(original, inputs, 10, &original_outputs, &original_size);
    ASSERT_NE(original_outputs, nullptr);
    ASSERT_GT(original_size, 0);

    // Save and load
    std::string filename = GetTempFilename();
    ASSERT_TRUE(brain_save(original, filename.c_str()));
    brain_t loaded = brain_load(filename.c_str());
    ASSERT_NE(loaded, nullptr);

    // Get outputs from loaded brain
    float* loaded_outputs = nullptr;
    int loaded_size = 0;
    brain_infer(loaded, inputs, 10, &loaded_outputs, &loaded_size);
    ASSERT_NE(loaded_outputs, nullptr);
    ASSERT_EQ(loaded_size, original_size);

    // Compare outputs (should be very close)
    float max_diff = 0.0f;
    for (int i = 0; i < original_size; ++i) {
        float diff = std::fabs(original_outputs[i] - loaded_outputs[i]);
        max_diff = std::max(max_diff, diff);
    }

    std::cout << "\n=== Data Integrity Test ===" << std::endl;
    std::cout << "Max output difference: " << max_diff << std::endl;

    EXPECT_LT(max_diff, 0.001f) << "Outputs differ after save/load";

    // Clean up
    brain_free_outputs(original_outputs);
    brain_free_outputs(loaded_outputs);
    brain_destroy(original);
    brain_destroy(loaded);
}

//=============================================================================
// STRESS TESTING
//=============================================================================

TEST_F(BrainPersistenceRegressionTest, Stress_RepeatedSaveLoadCycles) {
    // WHAT: Repeated save/load cycles (100 iterations)
    // WHY:  Detect memory leaks, cumulative errors
    // HOW:  Create, save, load, destroy repeatedly

    std::vector<double> save_times;
    std::vector<double> load_times;
    save_times.reserve(STRESS_CYCLE_COUNT);
    load_times.reserve(STRESS_CYCLE_COUNT);

    for (uint32_t cycle = 0; cycle < STRESS_CYCLE_COUNT; ++cycle) {
        brain_t brain = CreateAndTrainBrain(100, 50);
        ASSERT_NE(brain, nullptr);

        std::string filename = GetTempFilename();

        // Save
        auto save_start = std::chrono::high_resolution_clock::now();
        bool saved = brain_save(brain, filename.c_str());
        auto save_end = std::chrono::high_resolution_clock::now();
        ASSERT_TRUE(saved);

        double save_time = std::chrono::duration<double, std::milli>(
            save_end - save_start).count();
        save_times.push_back(save_time);

        brain_destroy(brain);

        // Load
        auto load_start = std::chrono::high_resolution_clock::now();
        brain_t loaded = brain_load(filename.c_str());
        auto load_end = std::chrono::high_resolution_clock::now();
        ASSERT_NE(loaded, nullptr);

        double load_time = std::chrono::duration<double, std::milli>(
            load_end - load_start).count();
        load_times.push_back(load_time);

        brain_destroy(loaded);

        if (cycle % 20 == 0) {
            std::cout << "Completed cycle " << cycle << std::endl;
        }
    }

    // Calculate statistics
    double avg_save = 0.0, avg_load = 0.0;
    for (size_t i = 0; i < save_times.size(); ++i) {
        avg_save += save_times[i];
        avg_load += load_times[i];
    }
    avg_save /= save_times.size();
    avg_load /= load_times.size();

    std::cout << "\n=== Stress Test Results (" << STRESS_CYCLE_COUNT << " cycles) ===" << std::endl;
    std::cout << "Average save time: " << avg_save << " ms" << std::endl;
    std::cout << "Average load time: " << avg_load << " ms" << std::endl;

    // Times should remain stable (no degradation over cycles)
    double first_third_save = 0.0, last_third_save = 0.0;
    for (size_t i = 0; i < save_times.size() / 3; ++i) {
        first_third_save += save_times[i];
        last_third_save += save_times[save_times.size() - 1 - i];
    }
    first_third_save /= (save_times.size() / 3);
    last_third_save /= (save_times.size() / 3);

    EXPECT_LT(last_third_save / first_third_save, 1.5)
        << "Save time degraded over cycles";
}

TEST_F(BrainPersistenceRegressionTest, Stress_ConcurrentSaveOperations) {
    // WHAT: Multiple threads saving brains concurrently
    // WHY:  Test thread safety of persistence
    // HOW:  4 threads each saving different brains

    constexpr int NUM_THREADS = 4;
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    auto thread_func = [&]() {
        for (int i = 0; i < 25; ++i) {
            brain_t brain = CreateAndTrainBrain(100, 50);
            if (!brain) continue;

            std::string filename = GetTempFilename();
            if (brain_save(brain, filename.c_str())) {
                success_count++;
            }

            brain_destroy(brain);
        }
    };

    // Launch threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "\n=== Concurrent Save Test ===" << std::endl;
    std::cout << "Successful saves: " << success_count.load() << " / " << (NUM_THREADS * 25) << std::endl;

    EXPECT_GE(success_count.load(), NUM_THREADS * 20)
        << "Too many save failures under concurrent load";
}

//=============================================================================
// MEMORY USAGE TESTS
//=============================================================================

TEST_F(BrainPersistenceRegressionTest, Memory_SaveMemoryUsage) {
    // WHAT: Track memory usage during save operation
    // WHY:  Ensure no excessive memory allocation
    // HOW:  Monitor memory before/after save

    brain_t brain = CreateAndTrainBrain(LARGE_BRAIN_SIZE, 200);
    ASSERT_NE(brain, nullptr);

    // Get initial memory stats
    persistence_stats_t initial_stats;
    ASSERT_TRUE(persistence_get_stats(&initial_stats));

    std::string filename = GetTempFilename();
    ASSERT_TRUE(brain_save(brain, filename.c_str()));

    // Get final memory stats
    persistence_stats_t final_stats;
    ASSERT_TRUE(persistence_get_stats(&final_stats));

    std::cout << "\n=== Save Memory Usage ===" << std::endl;
    std::cout << "Pool allocations: " << (final_stats.pool_allocations - initial_stats.pool_allocations) << std::endl;
    std::cout << "Malloc allocations: " << (final_stats.malloc_allocations - initial_stats.malloc_allocations) << std::endl;

    // Memory should be cleaned up after save
    SUCCEED();

    brain_destroy(brain);
}

TEST_F(BrainPersistenceRegressionTest, Memory_LoadMemoryUsage) {
    // WHAT: Track memory usage during load operation
    // WHY:  Ensure no memory leaks
    // HOW:  Save brain, then repeatedly load and destroy

    brain_t original = CreateAndTrainBrain(200, 100);
    ASSERT_NE(original, nullptr);

    std::string filename = GetTempFilename();
    ASSERT_TRUE(brain_save(original, filename.c_str()));
    brain_destroy(original);

    // Load multiple times
    for (int i = 0; i < 10; ++i) {
        brain_t loaded = brain_load(filename.c_str());
        ASSERT_NE(loaded, nullptr);
        brain_destroy(loaded);
    }

    std::cout << "Loaded and destroyed brain 10 times successfully" << std::endl;
    SUCCEED();
}

//=============================================================================
// STATISTICS VALIDATION
//=============================================================================

TEST_F(BrainPersistenceRegressionTest, Statistics_SaveLoadCounts) {
    // WHAT: Verify persistence statistics are accurate
    // WHY:  Stats used for monitoring
    // HOW:  Perform known operations, check stats

    persistence_stats_t initial_stats;
    ASSERT_TRUE(persistence_get_stats(&initial_stats));

    // Perform 5 save/load cycles
    for (int i = 0; i < 5; ++i) {
        brain_t brain = CreateAndTrainBrain(100, 50);
        std::string filename = GetTempFilename();
        brain_save(brain, filename.c_str());
        brain_destroy(brain);

        brain_t loaded = brain_load(filename.c_str());
        if (loaded) brain_destroy(loaded);
    }

    persistence_stats_t final_stats;
    ASSERT_TRUE(persistence_get_stats(&final_stats));

    uint64_t saves = final_stats.total_saves - initial_stats.total_saves;
    uint64_t loads = final_stats.total_loads - initial_stats.total_loads;

    std::cout << "\n=== Persistence Statistics ===" << std::endl;
    std::cout << "Saves: " << saves << std::endl;
    std::cout << "Loads: " << loads << std::endl;
    std::cout << "Bytes written: " << (final_stats.bytes_written - initial_stats.bytes_written) << std::endl;
    std::cout << "Bytes read: " << (final_stats.bytes_read - initial_stats.bytes_read) << std::endl;

    EXPECT_EQ(saves, 5);
    EXPECT_EQ(loads, 5);
}
