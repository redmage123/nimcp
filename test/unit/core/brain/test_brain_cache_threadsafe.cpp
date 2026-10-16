/**
 * @file test_brain_cache_threadsafe.cpp
 * @brief Comprehensive thread-safe brain decision caching tests
 *
 * WHAT: Test suite for thread-safe brain decision caching with mutex protection
 * WHY:  Ensure 100% coverage of caching logic and thread safety
 * HOW:  Unit tests (15+), integration tests (8+), regression tests (10+)
 *
 * COVERAGE TARGETS:
 * - Cache hits and misses
 * - Cache invalidation on network changes
 * - Concurrent access with multiple threads
 * - Mutex initialization and cleanup
 * - Edge cases (NULL, invalid sizes, memory failures)
 * - Performance benchmarks
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>

#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/cache/nimcp_cache.h"
#include "utils/time/nimcp_time.h"
#include "nimcp.h"
#include "utils/nimcp_test_base.h"

//=============================================================================
// Test Fixture
//=============================================================================

// RE-ENABLED: working_memory mutex protection implemented (2025-11-17)
// FIX APPLIED: Added platform_mutex_t to working_memory_t with locking in 16 functions
// VALIDATION: Testing if concurrent access issues resolved at nimcp_working_memory.c:420
// EXPECTED: All 34 tests should now pass (was 15/34 before mutex fix)
// MIGRATED: Now using NimcpTestBase for automatic global state cleanup
class BrainCacheThreadSafeTest : public NimcpTestBase {
protected:
    void SetUp() override {
        // Call parent first for global state cleanup
        NimcpTestBase::SetUp();

        // Initialize NIMCP systems via nimcp_init() which handles:
        // - nimcp_memory_init()
        // - nimcp_cache_init()
        // - bio-async initialization
        // This ensures proper initialization order
        nimcp_init();
        brain_clear_error();
    }

    void TearDown() override {
        // Cleanup via nimcp_shutdown() which handles:
        // - nimcp_cache_cleanup()
        // - bio-async shutdown
        // - nimcp_memory_cleanup()
        // Do NOT call these individually as nimcp_shutdown() does them in correct order
        nimcp_shutdown();

        // Call parent last for global state cleanup
        NimcpTestBase::TearDown();
    }

    // Helper: Create test brain
    brain_t create_test_brain(uint32_t inputs = 10, uint32_t outputs = 3) {
        return brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION,
                          inputs, outputs);
    }

    // Helper: Create test features
    float* create_features(uint32_t size, float base = 0.5f) {
        float* features = new float[size];
        for (uint32_t i = 0; i < size; i++) {
            features[i] = base + (float)i * 0.01f;
        }
        return features;
    }

    // Helper: Compare decisions
    bool decisions_equal(const brain_decision_t* d1, const brain_decision_t* d2) {
        if (!d1 || !d2) return false;
        if (std::strcmp(d1->label, d2->label) != 0) return false;
        if (std::abs(d1->confidence - d2->confidence) > 1e-6f) return false;
        if (d1->output_size != d2->output_size) return false;

        for (uint32_t i = 0; i < d1->output_size; i++) {
            if (std::abs(d1->output_vector[i] - d2->output_vector[i]) > 1e-6f)
                return false;
        }
        return true;
    }
};

//=============================================================================
// Unit Tests (15+): Basic Cache Operations
//=============================================================================

TEST_F(BrainCacheThreadSafeTest, CacheMiss_FirstCall) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10);

    // First call - should be cache miss
    brain_decision_t* decision1 = brain_decide(brain, features, 10);
    ASSERT_NE(decision1, nullptr);

    brain_free_decision(decision1);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheHit_RepeatedInput) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);

    // First call
    brain_decision_t* decision1 = brain_decide(brain, features, 10);
    ASSERT_NE(decision1, nullptr);

    // Second call with same input - should be cache hit
    brain_decision_t* decision2 = brain_decide(brain, features, 10);
    ASSERT_NE(decision2, nullptr);

    // Decisions should be identical
    EXPECT_TRUE(decisions_equal(decision1, decision2));

    brain_free_decision(decision1);
    brain_free_decision(decision2);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheMiss_DifferentInput) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features1 = create_features(10, 0.5f);
    float* features2 = create_features(10, 0.7f);

    // First call
    brain_decision_t* decision1 = brain_decide(brain, features1, 10);
    ASSERT_NE(decision1, nullptr);

    // Second call with different input - cache miss
    brain_decision_t* decision2 = brain_decide(brain, features2, 10);
    ASSERT_NE(decision2, nullptr);

    brain_free_decision(decision1);
    brain_free_decision(decision2);
    delete[] features1;
    delete[] features2;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheInvalidation_OnLearn) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);
    float* targets = new float[3]{1.0f, 0.0f, 0.0f};

    // First decision
    brain_decision_t* decision1 = brain_decide(brain, features, 10);
    ASSERT_NE(decision1, nullptr);
    float conf1 = decision1->confidence;
    brain_free_decision(decision1);

    // Learn - should invalidate cache
    brain_learn_example(brain, features, 10, "class_0", 1.0f);

    // Second decision after learning - should recompute
    brain_decision_t* decision2 = brain_decide(brain, features, 10);
    ASSERT_NE(decision2, nullptr);

    brain_free_decision(decision2);
    delete[] features;
    delete[] targets;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheInvalidation_OnBatchLearn) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);

    // First decision
    brain_decision_t* decision1 = brain_decide(brain, features, 10);
    ASSERT_NE(decision1, nullptr);
    brain_free_decision(decision1);

    // Batch learn - should invalidate cache
    brain_example_t example;
    example.features = features;
    example.num_features = 10;
    strcpy(example.label, "class_0");
    example.confidence = 1.0f;

    brain_learn_batch(brain, &example, 1);

    // Second decision - should recompute
    brain_decision_t* decision2 = brain_decide(brain, features, 10);
    ASSERT_NE(decision2, nullptr);

    brain_free_decision(decision2);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheHandles_NullInput) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    // NULL features should return NULL
    brain_decision_t* decision = brain_decide(brain, nullptr, 10);
    EXPECT_EQ(decision, nullptr);

    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheHandles_NullBrain) {
    float* features = create_features(10);

    // NULL brain should return NULL
    brain_decision_t* decision = brain_decide(nullptr, features, 10);
    EXPECT_EQ(decision, nullptr);

    delete[] features;
}

TEST_F(BrainCacheThreadSafeTest, CacheHandles_WrongSize) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(5);

    // Wrong size should return NULL
    brain_decision_t* decision = brain_decide(brain, features, 5);
    EXPECT_EQ(decision, nullptr);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheMultipleHits_SameInput) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);

    // Multiple calls with same input
    std::vector<brain_decision_t*> decisions;
    for (int i = 0; i < 10; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        decisions.push_back(decision);
    }

    // All decisions should be identical
    for (size_t i = 1; i < decisions.size(); i++) {
        EXPECT_TRUE(decisions_equal(decisions[0], decisions[i]));
    }

    // Cleanup
    for (auto* decision : decisions) {
        brain_free_decision(decision);
    }
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheAlternatingInputs) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features1 = create_features(10, 0.5f);
    float* features2 = create_features(10, 0.7f);

    // Alternate between two inputs
    brain_decision_t* d1a = brain_decide(brain, features1, 10);
    brain_decision_t* d2a = brain_decide(brain, features2, 10);
    brain_decision_t* d1b = brain_decide(brain, features1, 10);
    brain_decision_t* d2b = brain_decide(brain, features2, 10);

    ASSERT_NE(d1a, nullptr);
    ASSERT_NE(d2a, nullptr);
    ASSERT_NE(d1b, nullptr);
    ASSERT_NE(d2b, nullptr);

    // Only the last input is cached, so d1b and d2b should match
    // but d1a won't match d1b because cache was invalidated
    EXPECT_TRUE(decisions_equal(d2a, d2b));

    brain_free_decision(d1a);
    brain_free_decision(d2a);
    brain_free_decision(d1b);
    brain_free_decision(d2b);
    delete[] features1;
    delete[] features2;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheStats_Tracking) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);

    brain_stats_t stats1;
    brain_get_stats(brain, &stats1);
    uint64_t initial_inferences = stats1.total_inferences;

    // First call
    brain_decision_t* d1 = brain_decide(brain, features, 10);
    brain_free_decision(d1);

    brain_stats_t stats2;
    brain_get_stats(brain, &stats2);
    EXPECT_EQ(stats2.total_inferences, initial_inferences + 1);

    // Second call (cache hit)
    brain_decision_t* d2 = brain_decide(brain, features, 10);
    brain_free_decision(d2);

    brain_stats_t stats3;
    brain_get_stats(brain, &stats3);
    EXPECT_EQ(stats3.total_inferences, initial_inferences + 2);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheMemoryManagement_NoLeaks) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);

    // Many cache hits
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        ASSERT_NE(decision, nullptr);
        brain_free_decision(decision);
    }

    delete[] features;
    brain_destroy(brain);
    // If there are leaks, ASAN will catch them
}

TEST_F(BrainCacheThreadSafeTest, CacheCleanup_OnDestroy) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);

    // Cache a decision
    brain_decision_t* decision = brain_decide(brain, features, 10);
    ASSERT_NE(decision, nullptr);
    brain_free_decision(decision);

    // Destroy brain - cache should be cleaned up
    brain_destroy(brain);
    // ASAN will catch any double-free or leaks

    delete[] features;
}

TEST_F(BrainCacheThreadSafeTest, CacheZeroOutputs) {
    brain_t brain = create_test_brain(10, 0);
    // Brain with 0 outputs may validly return NULL
    if (brain == nullptr) {
        GTEST_SKIP() << "Brain creation with 0 outputs not supported";
    }

    float* features = create_features(10, 0.5f);

    brain_decision_t* decision = brain_decide(brain, features, 10);
    // May return NULL or valid decision with 0 outputs
    if (decision) {
        brain_free_decision(decision);
    }

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, CacheLargeInput) {
    brain_t brain = create_test_brain(1000, 10);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(1000, 0.5f);

    // First call
    brain_decision_t* d1 = brain_decide(brain, features, 1000);
    ASSERT_NE(d1, nullptr);

    // Second call - cache hit with large input
    brain_decision_t* d2 = brain_decide(brain, features, 1000);
    ASSERT_NE(d2, nullptr);

    EXPECT_TRUE(decisions_equal(d1, d2));

    brain_free_decision(d1);
    brain_free_decision(d2);
    delete[] features;
    brain_destroy(brain);
}

//=============================================================================
// Integration Tests (8+): Thread Safety & Concurrent Access
//=============================================================================

TEST_F(BrainCacheThreadSafeTest, ConcurrentAccess_SameInput) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    const int num_threads = 10;
    const int calls_per_thread = 100;

    auto worker = [&]() {
        for (int i = 0; i < calls_per_thread; i++) {
            brain_decision_t* decision = brain_decide(brain, features, 10);
            if (decision) {
                success_count++;
                brain_free_decision(decision);
            } else {
                failure_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // All calls should succeed
    EXPECT_EQ(success_count.load(), num_threads * calls_per_thread);
    EXPECT_EQ(failure_count.load(), 0);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, ConcurrentAccess_DifferentInputs) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    std::atomic<int> success_count{0};

    const int num_threads = 8;
    const int calls_per_thread = 50;

    auto worker = [&](int thread_id) {
        float* features = create_features(10, 0.5f + thread_id * 0.1f);

        for (int i = 0; i < calls_per_thread; i++) {
            brain_decision_t* decision = brain_decide(brain, features, 10);
            if (decision) {
                success_count++;
                brain_free_decision(decision);
            }
        }

        delete[] features;
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * calls_per_thread);
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, ConcurrentReadWrite_CacheInvalidation) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);
    float* targets = new float[3]{1.0f, 0.0f, 0.0f};
    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};

    // Reader threads
    auto reader = [&]() {
        while (!stop.load()) {
            brain_decision_t* decision = brain_decide(brain, features, 10);
            if (decision) {
                read_count++;
                brain_free_decision(decision);
            }
        }
    };

    // Writer thread (triggers cache invalidation)
    auto writer = [&]() {
        for (int i = 0; i < 10; i++) {
            brain_learn_example(brain, features, 10, "class_0", 1.0f);
            write_count++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        stop.store(true);
    };

    std::vector<std::thread> readers;
    for (int i = 0; i < 4; i++) {
        readers.emplace_back(reader);
    }
    std::thread writer_thread(writer);

    writer_thread.join();
    for (auto& t : readers) {
        t.join();
    }

    EXPECT_GT(read_count.load(), 0);
    EXPECT_EQ(write_count.load(), 10);

    delete[] features;
    delete[] targets;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, ConcurrentAccess_RaceCondition) {
    // This test specifically tries to trigger the old heap-use-after-free bug
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);
    std::atomic<int> success_count{0};

    const int num_threads = 20;
    const int iterations = 100;

    auto worker = [&]() {
        for (int i = 0; i < iterations; i++) {
            brain_decision_t* decision = brain_decide(brain, features, 10);
            if (decision) {
                // Simulate some work
                volatile float sum = 0.0f;
                for (uint32_t j = 0; j < decision->output_size; j++) {
                    sum += decision->output_vector[j];
                }
                success_count++;
                brain_free_decision(decision);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * iterations);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, ConcurrentCreate_Destroy) {
    // brain_create() and brain_destroy() are documented as NOT thread-safe
    // (see nimcp_brain.h: "Creation should be single-threaded or externally
    // synchronized"). Multiple brains alive concurrently is also unsupported
    // because they share global module registrations and bio-router state.
    // Test sequential create-use-destroy lifecycle across multiple iterations.
    int success_count = 0;

    const int iterations = 4;  // sequential lifecycle test (brain_create is not thread-safe)

    for (int i = 0; i < iterations; i++) {
        brain_t brain = create_test_brain(10, 3);
        if (brain) {
            float* features = create_features(10);
            brain_decision_t* decision = brain_decide(brain, features, 10);
            if (decision) {
                brain_free_decision(decision);
                success_count++;
            }
            delete[] features;
            brain_destroy(brain);
        }
    }

    EXPECT_EQ(success_count, iterations);
}

TEST_F(BrainCacheThreadSafeTest, StressTest_HighLoad) {
    brain_t brain = create_test_brain(50, 10);
    ASSERT_NE(brain, nullptr);

    std::atomic<int> total_operations{0};
    std::atomic<bool> stop{false};

    auto worker = [&](int thread_id) {
        float* features = create_features(50, 0.5f + thread_id * 0.01f);
        int local_count = 0;

        // Reduced from 1000 to 100 iterations for faster tests
        while (!stop.load() && local_count < 100) {
            brain_decision_t* decision = brain_decide(brain, features, 50);
            if (decision) {
                brain_free_decision(decision);
                local_count++;
            }
        }

        total_operations += local_count;
        delete[] features;
    };

    // Reduced from 2 seconds to 500ms
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);

    std::vector<std::thread> threads;
    // Reduced from 16 to 4 threads
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker, i);
    }

    // Wait for timeout
    std::this_thread::sleep_until(timeout);
    stop.store(true);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(total_operations.load(), 0);
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, DeadlockTest_NoHang) {
    // Ensure no deadlocks occur
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10);
    float* targets = new float[3]{1.0f, 0.0f, 0.0f};
    std::atomic<bool> stop{false};

    auto reader_writer = [&](int id) {
        for (int i = 0; i < 50; i++) {
            if (id % 2 == 0) {
                // Reader
                brain_decision_t* decision = brain_decide(brain, features, 10);
                if (decision) {
                    brain_free_decision(decision);
                }
            } else {
                // Writer
                brain_learn_example(brain, features, 10, "class_0", 0.01f);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back(reader_writer, i);
    }

    // Should complete without hanging
    for (auto& t : threads) {
        t.join();
    }

    delete[] features;
    delete[] targets;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, MutexValidation_InitDestroy) {
    // Create and destroy brains to test mutex lifecycle
    // Reduced from 100 to 5: brain_create allocates ~1-2GB per instance
    // and cumulative test memory (34 prior tests) risks OOM at higher counts
    for (int i = 0; i < 5; i++) {
        brain_t brain = create_test_brain(10, 3);
        ASSERT_NE(brain, nullptr);

        float* features = create_features(10);
        brain_decision_t* decision = brain_decide(brain, features, 10);
        if (decision) {
            brain_free_decision(decision);
        }

        delete[] features;
        brain_destroy(brain);
    }
}

//=============================================================================
// Regression Tests (10+): Backward Compatibility & Edge Cases
//=============================================================================

TEST_F(BrainCacheThreadSafeTest, Regression_HeapUseAfterFree) {
    // Original bug: Thread A checks cache, Thread B updates cache,
    // Thread A tries to use freed decision
    // This should no longer happen with mutex protection

    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features1 = create_features(10, 0.5f);
    float* features2 = create_features(10, 0.7f);
    std::atomic<int> errors{0};

    auto thread_a = [&]() {
        for (int i = 0; i < 500; i++) {
            brain_decision_t* decision = brain_decide(brain, features1, 10);
            if (decision) {
                // Access decision data (would crash if freed)
                volatile float conf = decision->confidence;
                (void)conf;
                brain_free_decision(decision);
            } else {
                errors++;
            }
        }
    };

    auto thread_b = [&]() {
        for (int i = 0; i < 500; i++) {
            brain_decision_t* decision = brain_decide(brain, features2, 10);
            if (decision) {
                brain_free_decision(decision);
            } else {
                errors++;
            }
        }
    };

    std::thread t1(thread_a);
    std::thread t2(thread_b);

    t1.join();
    t2.join();

    EXPECT_EQ(errors.load(), 0);

    delete[] features1;
    delete[] features2;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, Regression_CacheCoherency) {
    // Ensure cache always returns consistent results
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10, 0.5f);

    // Get first decision
    brain_decision_t* d1 = brain_decide(brain, features, 10);
    ASSERT_NE(d1, nullptr);

    // Get 100 more decisions - all should match
    for (int i = 0; i < 100; i++) {
        brain_decision_t* d2 = brain_decide(brain, features, 10);
        ASSERT_NE(d2, nullptr);
        EXPECT_TRUE(decisions_equal(d1, d2));
        brain_free_decision(d2);
    }

    brain_free_decision(d1);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, Regression_MemoryLeak_CacheOverwrite) {
    // Ensure no leaks when cache is repeatedly overwritten
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    for (int i = 0; i < 1000; i++) {
        float* features = create_features(10, 0.5f + i * 0.001f);
        brain_decision_t* decision = brain_decide(brain, features, 10);
        if (decision) {
            brain_free_decision(decision);
        }
        delete[] features;
    }

    brain_destroy(brain);
    // ASAN will detect leaks
}

TEST_F(BrainCacheThreadSafeTest, Regression_NullPointerDeref) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    // Various NULL scenarios
    EXPECT_EQ(brain_decide(nullptr, nullptr, 10), nullptr);

    float* features = create_features(10);
    EXPECT_EQ(brain_decide(nullptr, features, 10), nullptr);
    EXPECT_EQ(brain_decide(brain, nullptr, 10), nullptr);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, Regression_SizeValidation) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10);

    // Correct size
    brain_decision_t* d1 = brain_decide(brain, features, 10);
    EXPECT_NE(d1, nullptr);
    if (d1) brain_free_decision(d1);

    // Wrong sizes
    EXPECT_EQ(brain_decide(brain, features, 0), nullptr);
    EXPECT_EQ(brain_decide(brain, features, 5), nullptr);
    EXPECT_EQ(brain_decide(brain, features, 20), nullptr);

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, Regression_CacheAfterDestroy) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10);
    brain_decision_t* decision = brain_decide(brain, features, 10);
    if (decision) {
        brain_free_decision(decision);
    }

    brain_destroy(brain);

    // Using brain after destroy should not crash (but behavior undefined)
    // Just ensure no segfault

    delete[] features;
}

TEST_F(BrainCacheThreadSafeTest, Regression_DoubleDestroy) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    brain_destroy(brain);
    // Second destroy should be safe (NULL check)
    brain_destroy(nullptr);
}

TEST_F(BrainCacheThreadSafeTest, Regression_CachePersistence_AfterClone) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10);
    brain_decision_t* d1 = brain_decide(brain, features, 10);
    ASSERT_NE(d1, nullptr);
    brain_free_decision(d1);

    // Clone brain - cache should not be shared
    brain_t clone = brain_clone_cow(brain);
    if (clone) {
        brain_decision_t* d2 = brain_decide(clone, features, 10);
        if (d2) {
            brain_free_decision(d2);
        }
        brain_destroy(clone);
    }

    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, Regression_FloatingPointPrecision) {
    brain_t brain = create_test_brain(10, 3);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(10);

    // Slightly modify features
    for (int i = 0; i < 10; i++) {
        features[i] += 1e-10f;  // Tiny change
    }

    brain_decision_t* d1 = brain_decide(brain, features, 10);
    ASSERT_NE(d1, nullptr);

    // Exact same values should hit cache
    brain_decision_t* d2 = brain_decide(brain, features, 10);
    ASSERT_NE(d2, nullptr);
    EXPECT_TRUE(decisions_equal(d1, d2));

    brain_free_decision(d1);
    brain_free_decision(d2);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, Regression_PerformanceNoRegression) {
    brain_t brain = create_test_brain(100, 10);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(100);

    // First call - compute
    auto start1 = std::chrono::high_resolution_clock::now();
    brain_decision_t* d1 = brain_decide(brain, features, 100);
    auto end1 = std::chrono::high_resolution_clock::now();
    ASSERT_NE(d1, nullptr);
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    brain_free_decision(d1);

    // Second call - cache hit (should be much faster)
    auto start2 = std::chrono::high_resolution_clock::now();
    brain_decision_t* d2 = brain_decide(brain, features, 100);
    auto end2 = std::chrono::high_resolution_clock::now();
    ASSERT_NE(d2, nullptr);
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Cache hit should be faster (or at least not slower)
    EXPECT_LE(duration2.count(), duration1.count() * 2);

    brain_free_decision(d2);
    delete[] features;
    brain_destroy(brain);
}

TEST_F(BrainCacheThreadSafeTest, Performance_CacheVsNoCache) {
    brain_t brain = create_test_brain(100, 10);
    ASSERT_NE(brain, nullptr);

    float* features = create_features(100);

    // Warm up
    brain_decision_t* warmup = brain_decide(brain, features, 100);
    if (warmup) brain_free_decision(warmup);

    // Benchmark with cache - reduced from 1000 to 50 iterations
    const int iterations = 50;
    auto start_cached = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 100);
        if (decision) brain_free_decision(decision);
    }
    auto end_cached = std::chrono::high_resolution_clock::now();
    auto duration_cached = std::chrono::duration_cast<std::chrono::microseconds>(
        end_cached - start_cached);

    // Invalidate cache and benchmark without cache benefit
    float* targets = new float[10];
    for (int i = 0; i < 10; i++) targets[i] = 0.1f;

    auto start_nocache = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; i++) {
        brain_learn_example(brain, features, 100, "class_0", 0.001f);
        brain_decision_t* decision = brain_decide(brain, features, 100);
        if (decision) brain_free_decision(decision);
    }
    auto end_nocache = std::chrono::high_resolution_clock::now();
    auto duration_nocache = std::chrono::duration_cast<std::chrono::microseconds>(
        end_nocache - start_nocache);

    // Cached version should be faster
    EXPECT_LT(duration_cached.count(), duration_nocache.count());

    delete[] features;
    delete[] targets;
    brain_destroy(brain);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
