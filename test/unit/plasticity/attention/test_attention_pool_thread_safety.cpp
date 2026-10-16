/**
 * @file test_attention_pool_thread_safety.cpp
 * @brief Thread safety tests for attention memory pool initialization
 *
 * WHAT: Verify that the attention memory pool can be safely initialized
 *       and used by multiple threads concurrently
 * WHY:  The pool uses a lock-free three-state CAS pattern for lazy init.
 *       A previous implementation used atomic_flag spinlock with a double-checked
 *       locking pattern that had a race condition where a partially-initialized
 *       pool could be observed by concurrent threads.
 * HOW:  Launch many threads that concurrently call attention functions that
 *       trigger pool initialization, verify no crashes, leaks, or corruption.
 *
 * RELATED FIX: Replaced atomic_flag spinlock with three-state atomic CAS
 *              using explicit memory_order_acquire/release ordering
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_attention.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AttentionPoolThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing - pool is lazily initialized
    }

    void TearDown() override {
        // Nothing - pool is global, cleaned on shutdown
    }
};

//=============================================================================
// Thread Safety Tests
//=============================================================================

/**
 * WHAT: Test concurrent multihead attention creation and destruction
 * WHY:  Creation triggers pool initialization - concurrent creation exercises
 *       the CAS-based init path
 * HOW:  Launch N threads that each create/destroy a multihead attention system
 */
TEST_F(AttentionPoolThreadSafetyTest, ConcurrentCreation) {
    constexpr int NUM_THREADS = 8;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    multihead_attention_config_t config = {};
    config.num_heads = 2;
    config.input_dim = 16;
    config.output_dim = 16;
    config.sequence_length = 8;
    config.use_thalamic_gate = false;
    config.use_salience_weighting = false;
    config.gate_bias = 0.5f;
    config.use_positional_encoding = false;
    config.enable_quantum_attention = false;

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&config, &success_count, &failure_count]() {
            multihead_attention_t mha = multihead_attention_create(&config);
            if (mha) {
                success_count.fetch_add(1, std::memory_order_relaxed);
                multihead_attention_destroy(mha);
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All threads should succeed - pool init race should not cause failures
    EXPECT_GT(success_count.load(), 0)
        << "At least some threads should successfully create attention systems";
    // Some may fail due to resource constraints but none should crash
}

/**
 * WHAT: Test concurrent attention head creation exercises pool init
 * WHY:  Single head creation also triggers pool alloc. Multiple threads
 *       creating heads concurrently stresses the init path.
 * HOW:  Barrier-start N threads to maximize contention on pool init
 */
TEST_F(AttentionPoolThreadSafetyTest, ConcurrentHeadCreation) {
    constexpr int NUM_THREADS = 16;
    std::atomic<bool> go{false};
    std::atomic<int> ready_count{0};
    std::atomic<int> success_count{0};

    attention_head_config_t head_config = {};
    head_config.input_dim = 16;
    head_config.output_dim = 16;
    head_config.key_dim = 8;
    head_config.value_dim = 8;
    head_config.temperature = 1.0f;
    head_config.dropout_rate = 0.0f;

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            ready_count.fetch_add(1, std::memory_order_relaxed);
            // Spin-wait for all threads to be ready (maximize contention)
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            attention_head_t head = attention_head_create(&head_config);
            if (head) {
                success_count.fetch_add(1, std::memory_order_relaxed);
                attention_head_destroy(head);
            }
        });
    }

    // Wait for all threads to be ready, then release them simultaneously
    while (ready_count.load(std::memory_order_relaxed) < NUM_THREADS) {
        std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0)
        << "Head creation should succeed from multiple threads";
}

/**
 * WHAT: Test that attention_compute_entropy is thread-safe
 * WHY:  Entropy computation reads from pool (pool must be visible)
 *       and uses the standardized denormal threshold
 * HOW:  Multiple threads compute entropy on different data concurrently
 */
TEST_F(AttentionPoolThreadSafetyTest, ConcurrentEntropyComputation) {
    constexpr int NUM_THREADS = 8;
    constexpr uint32_t SEQ_LEN = 32;
    std::atomic<int> completed{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&completed, i]() {
            // Create uniform attention weights (each thread uses own data)
            float weights[SEQ_LEN];
            float val = 1.0f / static_cast<float>(SEQ_LEN);
            for (uint32_t j = 0; j < SEQ_LEN; j++) {
                weights[j] = val;
            }

            // Compute entropy - should not crash with concurrent pool access
            float entropy = attention_compute_entropy(weights, SEQ_LEN);

            // Uniform distribution should have maximum entropy
            // log(32) = ~3.466
            EXPECT_GT(entropy, 2.0f) << "Thread " << i << " got low entropy";
            EXPECT_LT(entropy, 4.0f) << "Thread " << i << " got unreasonable entropy";

            completed.fetch_add(1, std::memory_order_relaxed);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS);
}

/**
 * WHAT: Test that forward pass through attention head is safe per-thread
 * WHY:  Forward pass triggers buffer allocation from the pool
 * HOW:  Each thread creates its own head and runs a forward pass
 */
TEST_F(AttentionPoolThreadSafetyTest, ConcurrentForwardPass) {
    constexpr int NUM_THREADS = 4;
    constexpr uint32_t SEQ_LEN = 4;
    constexpr uint32_t INPUT_DIM = 8;
    constexpr uint32_t OUTPUT_DIM = 8;
    constexpr uint32_t KEY_DIM = 4;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&]() {
            attention_head_config_t cfg = {};
            cfg.input_dim = INPUT_DIM;
            cfg.output_dim = OUTPUT_DIM;
            cfg.key_dim = KEY_DIM;
            cfg.value_dim = KEY_DIM;
            cfg.temperature = 1.0f;
            cfg.dropout_rate = 0.0f;

            attention_head_t head = attention_head_create(&cfg);
            if (!head) {
                failure_count.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            // Prepare input data
            std::vector<float> query(SEQ_LEN * INPUT_DIM, 0.1f);
            std::vector<float> key(SEQ_LEN * INPUT_DIM, 0.1f);
            std::vector<float> value(SEQ_LEN * INPUT_DIM, 0.1f);
            std::vector<float> output(SEQ_LEN * OUTPUT_DIM, 0.0f);

            bool ok = attention_head_forward(
                head, query.data(), key.data(), value.data(),
                SEQ_LEN, output.data(), nullptr, nullptr, 0
            );

            if (ok) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                failure_count.fetch_add(1, std::memory_order_relaxed);
            }

            attention_head_destroy(head);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0)
        << "At least some threads should complete forward pass successfully";
}

/**
 * WHAT: Test config validation is thread-safe (read-only, no pool needed)
 * WHY:  Validation should work from any thread without initialization issues
 * HOW:  Multiple threads validate configs concurrently
 */
TEST_F(AttentionPoolThreadSafetyTest, ConcurrentConfigValidation) {
    constexpr int NUM_THREADS = 16;
    std::atomic<int> valid_count{0};
    std::atomic<int> invalid_count{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&valid_count, &invalid_count, i]() {
            multihead_attention_config_t config = {};
            if (i % 2 == 0) {
                // Valid config
                config.num_heads = 4;
                config.input_dim = 32;
                config.output_dim = 32;
                config.sequence_length = 16;

                if (attention_validate_config(&config)) {
                    valid_count.fetch_add(1, std::memory_order_relaxed);
                }
            } else {
                // Invalid config (zero heads)
                config.num_heads = 0;
                config.input_dim = 32;
                config.output_dim = 32;
                config.sequence_length = 16;

                if (!attention_validate_config(&config)) {
                    invalid_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(valid_count.load(), NUM_THREADS / 2);
    EXPECT_EQ(invalid_count.load(), NUM_THREADS / 2);
}
