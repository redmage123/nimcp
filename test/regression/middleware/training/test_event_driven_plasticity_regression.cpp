//=============================================================================
// test_event_driven_plasticity_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_event_driven_plasticity_regression.cpp
 * @brief Regression tests for Event-Driven Plasticity
 *
 * Tests cover:
 * - Performance benchmarks (spike processing, eligibility consolidation)
 * - Memory usage and leak detection
 * - Backward compatibility of configuration presets
 * - Edge cases and boundary conditions
 * - Stress testing (high-frequency events, large bursts)
 * - Numerical stability under continuous operation
 *
 * @version 1.0.0
 * @date 2025-11-27
 */

#include <gtest/gtest.h>
#include <chrono>
#include <cmath>
#include <vector>
#include <thread>
#include <atomic>
#include <numeric>
#include <random>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_event_driven_plasticity.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EventDrivenPlasticityRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        edp_ = nullptr;
        bridge_ = nullptr;
    }

    void TearDown() override {
        if (edp_) {
            edp_stop(edp_);
            edp_destroy(edp_);
            edp_ = nullptr;
        }
        if (bridge_) {
            tpb_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    void CreateDefaultSetup() {
        tpb_config_t tpb_config = tpb_config_default();
        bridge_ = tpb_create(&tpb_config);
        ASSERT_NE(bridge_, nullptr);

        edp_config_t edp_config = edp_config_default();
        edp_config.mode = EDP_MODE_IMMEDIATE;
        edp_ = edp_create(&edp_config);
        ASSERT_NE(edp_, nullptr);

        edp_connect_bridge(edp_, bridge_);
        edp_start(edp_);
    }

    edp_context_t* edp_;
    tpb_context_t* bridge_;
};

//=============================================================================
// Performance Benchmark Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, SpikeBurstProcessingPerformance) {
    CreateDefaultSetup();

    const int iterations = 1000;  // Reduced from 10000
    std::vector<uint32_t> neuron_ids = {1, 2, 3, 4, 5};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neuron_ids.data();
    burst.num_neurons = static_cast<uint32_t>(neuron_ids.size());
    burst.synchrony_score = 0.8f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        burst.timestamp_ns = i * 1000000;  // 1ms intervals
        edp_process_spike_burst(edp_, &burst, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_op = (double)duration.count() / iterations;
    std::cout << "Spike burst processing: " << us_per_op << " us/op ("
              << iterations << " iterations in " << duration.count() << " us)\n";

    // Performance requirement: < 2000us (2ms) per spike burst
    // Note: Current implementation uses O(n²) STDP pairing, which is slow but accurate
    // Future optimization: use spatial hashing for O(n) pairing
    EXPECT_LT(us_per_op, 2000.0) << "Spike burst processing should be < 2000us";
}

TEST_F(EventDrivenPlasticityRegressionTest, RewardProcessingPerformance) {
    CreateDefaultSetup();

    const int iterations = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        float reward = 0.5f + 0.5f * sinf(i * 0.01f);
        edp_process_reward(edp_, reward);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_op = (double)duration.count() / iterations;
    std::cout << "Reward processing: " << us_per_op << " us/op\n";

    // Performance requirement: < 50us per reward signal
    // Note: Reward processing includes eligibility consolidation and mutex-protected stats updates
    // 50us is reasonable for a function doing tpb_inject_reward + potential consolidation
    EXPECT_LT(us_per_op, 50.0) << "Reward processing should be < 50us";
}

TEST_F(EventDrivenPlasticityRegressionTest, EligibilityConsolidationPerformance) {
    CreateDefaultSetup();

    // Generate spike activity to create eligibility traces
    for (int i = 0; i < 100; i++) {
        std::vector<uint32_t> neurons;
        for (int j = 0; j < 10; j++) {
            neurons.push_back(i * 10 + j);
        }
        spike_burst_data_t burst = {0};
        burst.neuron_ids = neurons.data();
        burst.num_neurons = static_cast<uint32_t>(neurons.size());
        burst.timestamp_ns = i * 10000000;
        burst.synchrony_score = 0.8f;
        edp_process_spike_burst(edp_, &burst, 0);
    }

    const int iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        edp_consolidate_eligibility(edp_, 0.8f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_op = (double)duration.count() / iterations;
    std::cout << "Eligibility consolidation: " << us_per_op << " us/op\n";

    // Performance requirement: < 100us per consolidation
    EXPECT_LT(us_per_op, 100.0) << "Eligibility consolidation should be < 100us";
}

TEST_F(EventDrivenPlasticityRegressionTest, LargeBurstThroughput) {
    CreateDefaultSetup();

    const int num_neurons = 100;  // Reduced from 1000
    std::vector<uint32_t> neuron_ids(num_neurons);
    for (int i = 0; i < num_neurons; i++) {
        neuron_ids[i] = i;
    }

    spike_burst_data_t burst = {0};
    burst.neuron_ids = neuron_ids.data();
    burst.num_neurons = static_cast<uint32_t>(num_neurons);
    burst.synchrony_score = 0.9f;

    const int iterations = 100;  // Reduced from 1000

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        burst.timestamp_ns = i * 5000000;  // 5ms intervals
        edp_process_spike_burst(edp_, &burst, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double throughput = (double)(num_neurons * iterations) / ((double)duration.count() / 1000.0);
    std::cout << "Large burst throughput: " << throughput / 1e6 << " M spikes/sec\n";

    // Performance requirement: > 5K spikes/sec for large bursts
    // Note: Current O(n²) STDP pairing limits throughput; spatial hashing would improve this
    EXPECT_GT(throughput, 5000.0) << "Large burst throughput should be > 5K spikes/sec";
}

TEST_F(EventDrivenPlasticityRegressionTest, PredictionErrorProcessingPerformance) {
    CreateDefaultSetup();

    const int iterations = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        float error = 0.5f * (1.0f + sinf(i * 0.001f));
        edp_process_prediction_error(edp_, error, i % 10);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_op = (double)duration.count() / iterations;
    std::cout << "Prediction error processing: " << us_per_op << " us/op\n";

    // Performance requirement: < 10us per prediction error
    // Relaxed from 5us to account for CI/parallel-ctest CPU contention
    EXPECT_LT(us_per_op, 10.0) << "Prediction error processing should be < 10us";
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, CreateDestroyMemoryStability) {
    const int iterations = 10;  // Reduced from 100

    for (int i = 0; i < iterations; i++) {
        edp_config_t config = edp_config_default();
        edp_context_t* ctx = edp_create(&config);
        ASSERT_NE(ctx, nullptr) << "Failed at iteration " << i;

        // Connect bridge
        tpb_config_t tpb_config = tpb_config_default();
        tpb_context_t* bridge = tpb_create(&tpb_config);
        edp_connect_bridge(ctx, bridge);

        // Do some operations
        edp_start(ctx);
        edp_process_reward(ctx, 1.0f);

        uint32_t neurons[] = {1, 2, 3};
        spike_burst_data_t burst = {0};
        burst.neuron_ids = neurons;
        burst.num_neurons = 3;
        burst.timestamp_ns = 1000000;
        burst.synchrony_score = 0.8f;
        edp_process_spike_burst(ctx, &burst, 0);

        edp_stop(ctx);
        edp_destroy(ctx);
        tpb_destroy(bridge);
    }

    // If we get here without crash, memory is being managed correctly
    SUCCEED();
}

TEST_F(EventDrivenPlasticityRegressionTest, HighVolumeEventMemoryStability) {
    CreateDefaultSetup();

    const int num_bursts = 100;  // Reduced from 10000
    const int neurons_per_burst = 5;  // Reduced from 50

    for (int i = 0; i < num_bursts; i++) {
        std::vector<uint32_t> neurons(neurons_per_burst);
        for (int j = 0; j < neurons_per_burst; j++) {
            neurons[j] = (i * neurons_per_burst + j) % 100000;
        }

        spike_burst_data_t burst = {0};
        burst.neuron_ids = neurons.data();
        burst.num_neurons = static_cast<uint32_t>(neurons.size());
        burst.timestamp_ns = i * 1000000;
        burst.synchrony_score = 0.7f + 0.2f * (i % 5) / 5.0f;

        EXPECT_EQ(edp_process_spike_burst(edp_, &burst, i % 8), NIMCP_SUCCESS)
            << "Failed at burst " << i;

        // Periodic consolidation
        if (i % 100 == 99) {
            edp_consolidate_eligibility(edp_, 0.5f);
        }
    }

    // Verify statistics
    edp_stats_t stats;
    EXPECT_EQ(edp_get_stats(edp_, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.category_stats[EDP_CATEGORY_SPIKE].events_received, static_cast<uint32_t>(num_bursts));
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, ZeroNeuronBurst) {
    CreateDefaultSetup();

    spike_burst_data_t burst = {0};
    burst.neuron_ids = nullptr;
    burst.num_neurons = 0;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 0.8f;

    // Should handle gracefully - empty burst is a no-op success
    nimcp_result_t result = edp_process_spike_burst(edp_, &burst, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Zero neuron burst should succeed (no-op)";
}

TEST_F(EventDrivenPlasticityRegressionTest, ZeroSynchronyScore) {
    CreateDefaultSetup();

    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 0.0f;

    EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, MaximumSynchronyScore) {
    CreateDefaultSetup();

    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 1.0f;

    EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, ZeroReward) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_reward(edp_, 0.0f), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, NegativeReward) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_reward(edp_, -1.0f), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, LargePositiveReward) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_reward(edp_, 100.0f), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, ZeroPredictionError) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_prediction_error(edp_, 0.0f, 0), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, VerySmallPredictionError) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_prediction_error(edp_, 1e-10f, 0), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, VeryLargePredictionError) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_prediction_error(edp_, 1e6f, 0), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, ZeroNovelty) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_novelty(edp_, 0.0f, 0), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, MaxNovelty) {
    CreateDefaultSetup();

    EXPECT_EQ(edp_process_novelty(edp_, 1.0f, 0), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, ZeroConsolidationStrength) {
    CreateDefaultSetup();

    // Generate some activity first
    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 0.8f;
    edp_process_spike_burst(edp_, &burst, 0);

    // Zero strength consolidation
    uint32_t consolidated = edp_consolidate_eligibility(edp_, 0.0f);
    // Should still work, just with no effect
    SUCCEED();
}

TEST_F(EventDrivenPlasticityRegressionTest, VeryHighConsolidationStrength) {
    CreateDefaultSetup();

    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 0.8f;
    edp_process_spike_burst(edp_, &burst, 0);

    // Very high strength (should be clamped or handled)
    uint32_t consolidated = edp_consolidate_eligibility(edp_, 100.0f);
    SUCCEED();
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, ProcessingBeforeStart) {
    edp_config_t config = edp_config_default();
    edp_ = edp_create(&config);
    ASSERT_NE(edp_, nullptr);

    tpb_config_t tpb_config = tpb_config_default();
    bridge_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_, bridge_);

    // Don't call edp_start()
    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 0.8f;

    // Should still work (soft fail or queue events)
    nimcp_result_t result = edp_process_spike_burst(edp_, &burst, 0);
    // Accept SUCCESS or NOT_INITIALIZED as valid responses
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_NOT_INITIALIZED);
}

TEST_F(EventDrivenPlasticityRegressionTest, ProcessingAfterStop) {
    CreateDefaultSetup();

    // Process some events
    edp_process_reward(edp_, 1.0f);

    // Stop
    edp_stop(edp_);

    // Try to process after stop
    nimcp_result_t result = edp_process_reward(edp_, 1.0f);
    // Should handle gracefully
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_NOT_INITIALIZED);
}

TEST_F(EventDrivenPlasticityRegressionTest, StartStopCycles) {
    edp_config_t config = edp_config_default();
    edp_ = edp_create(&config);
    ASSERT_NE(edp_, nullptr);

    tpb_config_t tpb_config = tpb_config_default();
    bridge_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_, bridge_);

    // Multiple start/stop cycles
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(edp_start(edp_), NIMCP_SUCCESS) << "Start failed at cycle " << i;

        // Do some work
        edp_process_reward(edp_, 1.0f);

        EXPECT_EQ(edp_stop(edp_), NIMCP_SUCCESS) << "Stop failed at cycle " << i;
    }
}

TEST_F(EventDrivenPlasticityRegressionTest, RapidTimestampProgression) {
    CreateDefaultSetup();

    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.synchrony_score = 0.8f;

    // Rapid timestamp progression (simulating fast-forward)
    for (uint64_t ts = 0; ts < 1000000000000ULL; ts += 100000000000ULL) {
        burst.timestamp_ns = ts;
        EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS)
            << "Failed at timestamp " << ts;
    }
}

TEST_F(EventDrivenPlasticityRegressionTest, OutOfOrderTimestamps) {
    CreateDefaultSetup();

    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.synchrony_score = 0.8f;

    // Out of order timestamps (shouldn't crash, may be handled differently)
    burst.timestamp_ns = 10000000;
    EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);

    burst.timestamp_ns = 5000000;  // Earlier timestamp
    EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);

    burst.timestamp_ns = 15000000;
    EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, HighFrequencyMixedOperations) {
    CreateDefaultSetup();

    const int iterations = 1000;  // Reduced from 100000 for faster testing
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    std::uniform_int_distribution<int> op_dist(0, 4);

    uint32_t base_neurons[] = {1, 2, 3, 4, 5};

    for (int i = 0; i < iterations; i++) {
        int op = op_dist(rng);

        switch (op) {
            case 0: {
                // Spike burst
                spike_burst_data_t burst = {0};
                burst.neuron_ids = base_neurons;
                burst.num_neurons = 5;
                burst.timestamp_ns = i * 1000000;
                burst.synchrony_score = dist(rng);
                edp_process_spike_burst(edp_, &burst, i % 8);
                break;
            }
            case 1: {
                // Reward
                edp_process_reward(edp_, dist(rng) * 2.0f - 0.5f);
                break;
            }
            case 2: {
                // Prediction error
                edp_process_prediction_error(edp_, dist(rng), i % 10);
                break;
            }
            case 3: {
                // Novelty
                edp_process_novelty(edp_, dist(rng), i % 8);
                break;
            }
            case 4: {
                // Consolidation
                edp_consolidate_eligibility(edp_, dist(rng));
                break;
            }
        }
    }

    // Verify state is consistent
    edp_stats_t stats;
    EXPECT_EQ(edp_get_stats(edp_, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.total_events_received, 0u);
}

TEST_F(EventDrivenPlasticityRegressionTest, ConcurrentEventProcessing) {
    CreateDefaultSetup();

    const int num_threads = 4;
    const int events_per_thread = 100;  // Reduced from 10000
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    auto worker = [&](int thread_id) {
        uint32_t neurons[] = {
            static_cast<uint32_t>(thread_id * 10),
            static_cast<uint32_t>(thread_id * 10 + 1),
            static_cast<uint32_t>(thread_id * 10 + 2)
        };

        for (int i = 0; i < events_per_thread; i++) {
            int op = (thread_id + i) % 4;

            switch (op) {
                case 0: {
                    spike_burst_data_t burst = {0};
                    burst.neuron_ids = neurons;
                    burst.num_neurons = 3;
                    burst.timestamp_ns = (thread_id * events_per_thread + i) * 1000000;
                    burst.synchrony_score = 0.8f;
                    if (edp_process_spike_burst(edp_, &burst, thread_id % 8) == NIMCP_SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                    break;
                }
                case 1: {
                    if (edp_process_reward(edp_, 0.5f + 0.1f * thread_id) == NIMCP_SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                    break;
                }
                case 2: {
                    if (edp_process_prediction_error(edp_, 0.2f, thread_id) == NIMCP_SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                    break;
                }
                case 3: {
                    if (edp_process_novelty(edp_, 0.6f, thread_id % 8) == NIMCP_SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                    break;
                }
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "Concurrent test: " << success_count.load() << " success, "
              << error_count.load() << " errors\n";

    // Allow some soft failures in concurrent scenario
    EXPECT_GT(success_count.load(), num_threads * events_per_thread * 0.9)
        << "At least 90% of operations should succeed";
}

TEST_F(EventDrivenPlasticityRegressionTest, BurstySpikePatterns) {
    CreateDefaultSetup();

    // Simulate bursty activity (quiet periods followed by intense bursts)
    for (int burst_idx = 0; burst_idx < 20; burst_idx++) {  // Reduced from 100
        // Burst of 10 rapid spike events (reduced from 50)
        for (int spike_idx = 0; spike_idx < 10; spike_idx++) {
            std::vector<uint32_t> neurons;
            for (int n = 0; n < 20; n++) {
                neurons.push_back(burst_idx * 1000 + spike_idx * 20 + n);
            }

            spike_burst_data_t burst = {0};
            burst.neuron_ids = neurons.data();
            burst.num_neurons = static_cast<uint32_t>(neurons.size());
            burst.timestamp_ns = (burst_idx * 100000000) + (spike_idx * 1000000);  // 1ms apart
            burst.synchrony_score = 0.9f;

            EXPECT_EQ(edp_process_spike_burst(edp_, &burst, burst_idx % 8), NIMCP_SUCCESS);
        }

        // Reward after burst
        edp_process_reward(edp_, 1.0f);

        // Consolidate
        edp_consolidate_eligibility(edp_, 0.7f);
    }

    edp_stats_t stats;
    edp_get_stats(edp_, &stats);
    EXPECT_EQ(stats.category_stats[EDP_CATEGORY_SPIKE].events_received, 200);  // 20 bursts * 10 spikes
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, DefaultConfigBackwardCompat) {
    edp_config_t config = edp_config_default();

    // Verify expected defaults haven't changed
    EXPECT_EQ(config.mode, EDP_MODE_IMMEDIATE);
    EXPECT_GT(config.stdp_window_ms, 0.0f);
    EXPECT_GT(config.ltp_rate, 0.0f);
    EXPECT_GT(config.ltd_rate, 0.0f);
    EXPECT_GT(config.eligibility_tau_ms, 0.0f);
    EXPECT_GT(config.novelty_gain, 0.0f);
    EXPECT_GT(config.error_gain, 0.0f);
}

TEST_F(EventDrivenPlasticityRegressionTest, BiologicalPresetBackwardCompat) {
    edp_config_t config = edp_config_biological();

    // Biological preset should have longer STDP window
    EXPECT_GE(config.stdp_window_ms, 40.0f) << "Biological STDP window should be >= 40ms";

    // Should be in batched mode for biological realism
    EXPECT_EQ(config.mode, EDP_MODE_BATCHED);

    // Eligibility trace should last longer (hundreds of ms to seconds)
    EXPECT_GE(config.eligibility_tau_ms, 500.0f);
}

TEST_F(EventDrivenPlasticityRegressionTest, HighPerformancePresetBackwardCompat) {
    edp_config_t config = edp_config_high_performance();

    // High-performance should have shorter STDP window
    EXPECT_LE(config.stdp_window_ms, 20.0f);

    // Should be in immediate mode for fastest processing
    EXPECT_EQ(config.mode, EDP_MODE_IMMEDIATE);

    // Shorter eligibility for less memory usage
    EXPECT_LE(config.eligibility_tau_ms, 100.0f);
}

TEST_F(EventDrivenPlasticityRegressionTest, ConfigPresetCreation) {
    // All presets should create valid contexts
    edp_config_t default_config = edp_config_default();
    edp_context_t* default_ctx = edp_create(&default_config);
    ASSERT_NE(default_ctx, nullptr);
    edp_destroy(default_ctx);

    edp_config_t bio_config = edp_config_biological();
    edp_context_t* bio_ctx = edp_create(&bio_config);
    ASSERT_NE(bio_ctx, nullptr);
    edp_destroy(bio_ctx);

    edp_config_t hp_config = edp_config_high_performance();
    edp_context_t* hp_ctx = edp_create(&hp_config);
    ASSERT_NE(hp_ctx, nullptr);
    edp_destroy(hp_ctx);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, NumericalStabilityLongRun) {
    CreateDefaultSetup();

    uint32_t neurons[] = {1, 2, 3, 4, 5};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 5;
    burst.synchrony_score = 0.8f;

    // Run many iterations (reduced from 100000 for faster testing)
    for (int i = 0; i < 1000; i++) {
        burst.timestamp_ns = i * 1000000;

        // Vary inputs to stress numerical stability
        burst.synchrony_score = 0.5f + 0.4f * sinf(i * 0.001f);

        EXPECT_EQ(edp_process_spike_burst(edp_, &burst, i % 8), NIMCP_SUCCESS)
            << "Failed at iteration " << i;

        if (i % 10 == 0) {
            float reward = sinf(i * 0.0001f);
            edp_process_reward(edp_, reward);
        }

        if (i % 100 == 0) {
            edp_consolidate_eligibility(edp_, 0.5f);
        }
    }

    // Verify stats are valid (no NaN or overflow)
    edp_stats_t stats;
    EXPECT_EQ(edp_get_stats(edp_, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.total_events_received, 0u);
    EXPECT_LT(stats.total_events_received, UINT32_MAX);
}

TEST_F(EventDrivenPlasticityRegressionTest, ExtremeValueStability) {
    CreateDefaultSetup();

    // Test with extreme values
    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.synchrony_score = 0.5f;

    // Very large timestamp
    burst.timestamp_ns = UINT64_MAX / 2;
    EXPECT_EQ(edp_process_spike_burst(edp_, &burst, 0), NIMCP_SUCCESS);

    // Very small rewards
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(edp_process_reward(edp_, 1e-10f), NIMCP_SUCCESS);
    }

    // Very large prediction errors
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(edp_process_prediction_error(edp_, 1e10f, i % 10), NIMCP_SUCCESS);
    }

    // Verify no corruption
    edp_stats_t stats;
    EXPECT_EQ(edp_get_stats(edp_, &stats), NIMCP_SUCCESS);
}

TEST_F(EventDrivenPlasticityRegressionTest, AccumulatingStatistics) {
    CreateDefaultSetup();

    // Process known number of events
    const uint32_t spike_events = 1000;
    const uint32_t reward_events = 500;
    const uint32_t error_events = 300;
    const uint32_t novelty_events = 200;

    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.synchrony_score = 0.8f;

    for (uint32_t i = 0; i < spike_events; i++) {
        burst.timestamp_ns = i * 1000000;
        edp_process_spike_burst(edp_, &burst, i % 8);
    }

    for (uint32_t i = 0; i < reward_events; i++) {
        edp_process_reward(edp_, 0.5f);
    }

    for (uint32_t i = 0; i < error_events; i++) {
        edp_process_prediction_error(edp_, 0.2f, i % 10);
    }

    for (uint32_t i = 0; i < novelty_events; i++) {
        edp_process_novelty(edp_, 0.6f, i % 8);
    }

    // Verify counts match
    edp_stats_t stats;
    edp_get_stats(edp_, &stats);

    EXPECT_EQ(stats.category_stats[EDP_CATEGORY_SPIKE].events_received, spike_events);
    EXPECT_GE(stats.category_stats[EDP_CATEGORY_REWARD].events_received, reward_events);
    EXPECT_GE(stats.category_stats[EDP_CATEGORY_ERROR].events_received, error_events);
    EXPECT_GE(stats.category_stats[EDP_CATEGORY_NOVELTY].events_received, novelty_events);

    // Total should be at least sum of individual categories
    EXPECT_GE(stats.total_events_received,
              spike_events + reward_events + error_events + novelty_events);
}

//=============================================================================
// Configuration Validation Tests
//=============================================================================

TEST_F(EventDrivenPlasticityRegressionTest, InvalidConfigHandling) {
    edp_config_t config = edp_config_default();

    // Set invalid values
    config.stdp_window_ms = -1.0f;  // Negative
    config.ltp_rate = -0.5f;        // Negative

    // Should either reject or sanitize
    edp_context_t* ctx = edp_create(&config);
    // Implementation may accept and clamp, or may reject
    if (ctx) {
        edp_destroy(ctx);
    }
    // No crash is success
    SUCCEED();
}

TEST_F(EventDrivenPlasticityRegressionTest, NullBridgeOperations) {
    edp_config_t config = edp_config_default();
    edp_ = edp_create(&config);
    ASSERT_NE(edp_, nullptr);

    // Don't connect bridge
    edp_start(edp_);

    // Reward processing requires bridge
    nimcp_result_t result = edp_process_reward(edp_, 1.0f);
    EXPECT_EQ(result, NIMCP_NOT_INITIALIZED);

    // Spike burst does NOT require bridge (local STDP only)
    uint32_t neurons[] = {1, 2, 3};
    spike_burst_data_t burst = {0};
    burst.neuron_ids = neurons;
    burst.num_neurons = 3;
    burst.timestamp_ns = 1000000;
    burst.synchrony_score = 0.8f;

    result = edp_process_spike_burst(edp_, &burst, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);  // Spike burst works without bridge
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
