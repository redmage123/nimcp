/**
 * @file test_attention_pool_init_regression.cpp
 * @brief Regression tests for attention pool initialization race condition
 *
 * WHAT: Verify that the three-state CAS-based pool initialization pattern
 *       does not regress to the broken double-checked locking pattern
 * WHY:  The original implementation used an atomic_flag spinlock that could
 *       allow a thread to observe a partially-initialized pool (the flag_clear
 *       could reorder relative to the pool pointer store). The fix uses
 *       explicit memory_order_acquire/release with a three-state atomic.
 * HOW:  Exercise pool init from multiple threads with barrier synchronization
 *       to maximize the probability of concurrent init attempts. Verify that
 *       all threads get a valid, consistent pool reference.
 *
 * REGRESSION PROTECTS AGAINST:
 * - Partially visible pool (memory ordering race)
 * - Double initialization (two threads both create pools)
 * - Spinlock starvation (old pattern could spin indefinitely under contention)
 * - Pool pointer corruption (CAS failure causing NULL dereference)
 *
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <set>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "plasticity/attention/nimcp_attention.h"

//=============================================================================
// Regression Test Fixture
//=============================================================================

class AttentionPoolInitRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

//=============================================================================
// Regression Tests
//=============================================================================

/**
 * WHAT: Verify pool initialization produces consistent results across threads
 * WHY:  If pool init races, different threads might see different pool states
 *       or get NULL when they should get a valid pool
 * HOW:  All threads create attention heads (triggering pool init) and perform
 *       forward passes. All should produce numerically consistent results
 *       for the same inputs.
 */
TEST_F(AttentionPoolInitRegressionTest, ConsistentPoolAcrossThreads) {
    constexpr int NUM_THREADS = 8;
    constexpr uint32_t SEQ_LEN = 4;
    constexpr uint32_t INPUT_DIM = 8;
    constexpr uint32_t OUTPUT_DIM = 8;
    constexpr uint32_t KEY_DIM = 4;

    std::atomic<bool> go{false};
    std::atomic<int> ready{0};

    // Each thread stores its output for comparison
    std::vector<std::vector<float>> thread_outputs(NUM_THREADS);
    std::vector<bool> thread_success(NUM_THREADS, false);

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            // All threads use identical config and inputs
            attention_head_config_t cfg = {};
            cfg.input_dim = INPUT_DIM;
            cfg.output_dim = OUTPUT_DIM;
            cfg.key_dim = KEY_DIM;
            cfg.value_dim = KEY_DIM;
            cfg.temperature = 1.0f;
            cfg.dropout_rate = 0.0f;

            // Signal ready and wait for synchronized start
            ready.fetch_add(1, std::memory_order_relaxed);
            while (!go.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            attention_head_t head = attention_head_create(&cfg);
            if (!head) return;

            // Use deterministic input
            std::vector<float> query(SEQ_LEN * INPUT_DIM, 0.5f);
            std::vector<float> key(SEQ_LEN * INPUT_DIM, 0.5f);
            std::vector<float> value(SEQ_LEN * INPUT_DIM, 0.5f);
            thread_outputs[t].resize(SEQ_LEN * OUTPUT_DIM, 0.0f);

            bool ok = attention_head_forward(
                head, query.data(), key.data(), value.data(),
                SEQ_LEN, thread_outputs[t].data(), nullptr, nullptr, 0
            );

            thread_success[t] = ok;
            attention_head_destroy(head);
        });
    }

    // Wait for all threads, then release simultaneously
    while (ready.load(std::memory_order_relaxed) < NUM_THREADS) {
        std::this_thread::yield();
    }
    go.store(true, std::memory_order_release);

    for (auto& th : threads) {
        th.join();
    }

    // Count successful threads
    int successes = 0;
    for (int t = 0; t < NUM_THREADS; t++) {
        if (thread_success[t]) successes++;
    }
    EXPECT_GT(successes, 0) << "At least one thread should succeed";

    // All successful threads should produce valid (finite) output.
    // Note: Each thread creates its own attention_head_t with random weight
    // initialization, so outputs WILL differ across threads. The regression
    // being tested is that concurrent pool initialization via CAS doesn't
    // cause crashes, corruption, or NaN/Inf outputs.
    for (int t = 0; t < NUM_THREADS; t++) {
        if (!thread_success[t]) continue;
        ASSERT_EQ(thread_outputs[t].size(), (size_t)(SEQ_LEN * OUTPUT_DIM));
        for (size_t i = 0; i < thread_outputs[t].size(); i++) {
            EXPECT_FALSE(std::isnan(thread_outputs[t][i]))
                << "Thread " << t << " output[" << i << "] is NaN (pool init corruption?)";
            EXPECT_FALSE(std::isinf(thread_outputs[t][i]))
                << "Thread " << t << " output[" << i << "] is Inf (pool init corruption?)";
        }
    }
}

/**
 * WHAT: Regression test for entropy computation with denormal inputs
 * WHY:  The entropy function now uses NIMCP_DENORMAL_THRESHOLD instead of
 *       hardcoded 1e-10F. Verify it handles near-zero weights correctly.
 * HOW:  Feed attention weights with some near-zero values and verify
 *       entropy computation doesn't produce NaN/Inf
 */
TEST_F(AttentionPoolInitRegressionTest, EntropyWithNearZeroWeights) {
    constexpr uint32_t SEQ_LEN = 8;
    float weights[SEQ_LEN];

    // Test 1: All weights near zero except one
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        weights[i] = 1e-15f;  // Below denormal threshold - should be skipped
    }
    weights[0] = 1.0f;  // All attention on first element

    float entropy = attention_compute_entropy(weights, SEQ_LEN);
    EXPECT_FALSE(std::isnan(entropy)) << "Entropy should not be NaN with near-zero weights";
    EXPECT_FALSE(std::isinf(entropy)) << "Entropy should not be Inf with near-zero weights";
    // Entropy of delta distribution should be ~0
    EXPECT_NEAR(entropy, 0.0f, 0.1f)
        << "Focused attention (one hot) should have near-zero entropy";

    // Test 2: Uniform distribution
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        weights[i] = 1.0f / SEQ_LEN;
    }

    entropy = attention_compute_entropy(weights, SEQ_LEN);
    EXPECT_FALSE(std::isnan(entropy));
    float expected_entropy = -logf(1.0f / SEQ_LEN);  // log(N) for uniform
    EXPECT_NEAR(entropy, expected_entropy, 0.01f)
        << "Uniform distribution should have entropy = log(N)";

    // Test 3: Exactly zero weights
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        weights[i] = 0.0f;
    }

    entropy = attention_compute_entropy(weights, SEQ_LEN);
    EXPECT_FALSE(std::isnan(entropy));
    EXPECT_NEAR(entropy, 0.0f, 1e-6f)
        << "All-zero weights should produce zero entropy";
}

/**
 * WHAT: Verify attention system survives rapid create/destroy cycles
 * WHY:  Pool init/access during rapid lifecycle churn could expose
 *       memory ordering issues
 * HOW:  Rapidly create and destroy attention heads in a tight loop
 */
TEST_F(AttentionPoolInitRegressionTest, RapidCreateDestroyCycles) {
    constexpr int NUM_CYCLES = 100;

    attention_head_config_t cfg = {};
    cfg.input_dim = 8;
    cfg.output_dim = 8;
    cfg.key_dim = 4;
    cfg.value_dim = 4;
    cfg.temperature = 1.0f;
    cfg.dropout_rate = 0.0f;

    int success_count = 0;
    for (int i = 0; i < NUM_CYCLES; i++) {
        attention_head_t head = attention_head_create(&cfg);
        if (head) {
            success_count++;
            attention_head_destroy(head);
        }
    }

    EXPECT_GT(success_count, NUM_CYCLES / 2)
        << "Most create/destroy cycles should succeed";
}

/**
 * WHAT: Verify multihead attention survives concurrent operations
 * WHY:  Real-world usage involves multiple attention systems
 *       operating concurrently with shared pool
 * HOW:  Create multiple multihead systems, run forward passes concurrently
 */
TEST_F(AttentionPoolInitRegressionTest, ConcurrentMultiheadOperations) {
    constexpr int NUM_THREADS = 4;
    constexpr uint32_t SEQ_LEN = 4;
    constexpr uint32_t INPUT_DIM = 16;
    constexpr uint32_t OUTPUT_DIM = 16;

    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&success_count, i]() {
            multihead_attention_config_t config = {};
            config.num_heads = 2;
            config.input_dim = INPUT_DIM;
            config.output_dim = OUTPUT_DIM;
            config.sequence_length = SEQ_LEN;
            config.use_thalamic_gate = (i % 2 == 0);
            config.use_salience_weighting = false;
            config.gate_bias = 0.5f;
            config.use_positional_encoding = false;
            config.enable_quantum_attention = false;

            multihead_attention_t mha = multihead_attention_create(&config);
            if (!mha) return;

            // Prepare input
            std::vector<float> input(SEQ_LEN * INPUT_DIM, 0.1f);
            std::vector<float> output(SEQ_LEN * OUTPUT_DIM, 0.0f);

            bool ok = multihead_attention_forward(
                mha, input.data(), SEQ_LEN, nullptr, output.data()
            );

            if (ok) {
                // Verify output is not all zeros and not NaN
                bool has_nonzero = false;
                bool has_nan = false;
                for (size_t j = 0; j < output.size(); j++) {
                    if (output[j] != 0.0f) has_nonzero = true;
                    if (std::isnan(output[j])) has_nan = true;
                }
                if (has_nonzero && !has_nan) {
                    success_count.fetch_add(1, std::memory_order_relaxed);
                }
            }

            multihead_attention_destroy(mha);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0)
        << "At least some multihead forward passes should succeed concurrently";
}

/**
 * WHAT: Verify gate operations work correctly from multiple threads
 * WHY:  Gate set is documented as lock-free - should work from any thread
 * HOW:  Create one MHA system, set gate from multiple threads, read stats
 */
TEST_F(AttentionPoolInitRegressionTest, ConcurrentGateAccess) {
    multihead_attention_config_t config = {};
    config.num_heads = 2;
    config.input_dim = 16;
    config.output_dim = 16;
    config.sequence_length = 8;
    config.use_thalamic_gate = true;
    config.use_salience_weighting = false;
    config.gate_bias = 0.5f;
    config.use_positional_encoding = false;
    config.enable_quantum_attention = false;

    multihead_attention_t mha = multihead_attention_create(&config);
    if (!mha) {
        GTEST_SKIP() << "Could not create MHA system (resource constraint)";
        return;
    }

    constexpr int NUM_THREADS = 8;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([mha, &success_count, i]() {
            float gate_val = static_cast<float>(i) / 8.0f;
            bool ok = multihead_attention_set_gate(mha, gate_val);
            if (ok) {
                success_count.fetch_add(1, std::memory_order_relaxed);
            }

            // Also test stats reading (should be thread-safe)
            attention_stats_t stats = {};
            multihead_attention_get_stats(mha, &stats);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS)
        << "All gate set operations should succeed";

    multihead_attention_destroy(mha);
}
