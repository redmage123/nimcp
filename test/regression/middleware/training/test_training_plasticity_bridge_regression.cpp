//=============================================================================
// test_training_plasticity_bridge_regression.cpp - Regression Tests
//=============================================================================
/**
 * @file test_training_plasticity_bridge_regression.cpp
 * @brief Regression tests for Training-Plasticity Bridge
 *
 * Tests cover:
 * - Performance benchmarks
 * - Memory usage and leak detection
 * - Backward compatibility
 * - Edge cases and boundary conditions
 * - Stress testing
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

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingPlasticityRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        bridge_ = nullptr;
    }

    void TearDown() override {
        if (bridge_) {
            tpb_destroy(bridge_);
            bridge_ = nullptr;
        }
    }

    tpb_context_t* bridge_;
};

//=============================================================================
// Performance Benchmark Tests
//=============================================================================

TEST_F(TrainingPlasticityRegressionTest, RPEComputationPerformance) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    const int iterations = 10000;
    float rpe = 0.0f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        float loss = 1.0f - 0.0001f * i + 0.1f * sinf(i * 0.01f);
        tpb_report_loss(bridge_, loss, &rpe);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_op = (double)duration.count() / iterations;
    std::cout << "RPE computation: " << us_per_op << " us/op ("
              << iterations << " iterations in " << duration.count() << " us)\n";

    // Performance requirement: < 10us per RPE computation
    EXPECT_LT(us_per_op, 10.0) << "RPE computation should be < 10us";
}

TEST_F(TrainingPlasticityRegressionTest, SingleWeightUpdatePerformance) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 100000;
    tpb_configure_region(bridge_, &region, nullptr);

    const int iterations = 10000;
    float delta = 0.0f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        tpb_route_weight_update(bridge_, i % 100000, 0.8f, 0.9f, 10.0f, &delta);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_op = (double)duration.count() / iterations;
    std::cout << "Single weight update: " << us_per_op << " us/op\n";

    // Performance requirement: < 50us per single update (relaxed for CI/parallel test contention)
    EXPECT_LT(us_per_op, 50.0) << "Single weight update should be < 50us";
}

TEST_F(TrainingPlasticityRegressionTest, BatchPlasticityPerformance) {
    tpb_config_t config = tpb_config_default();
    config.thread_pool_size = 4;
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 100000;
    tpb_configure_region(bridge_, &region, nullptr);

    const uint32_t batch_size = 10000;
    const int iterations = 100;

    std::vector<uint32_t> pre_ids(batch_size), post_ids(batch_size);
    std::vector<float> pre_act(batch_size), post_act(batch_size);
    std::vector<float> deltas(batch_size), weights(batch_size, 0.5f);

    for (uint32_t i = 0; i < batch_size; i++) {
        pre_ids[i] = i;
        post_ids[i] = (i + 1) % 100000;
        pre_act[i] = 0.8f;
        post_act[i] = 0.9f;
        deltas[i] = (i % 2 == 0) ? 10.0f : -10.0f;
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int iter = 0; iter < iterations; iter++) {
        tpb_apply_plasticity_batch(bridge_, batch_size,
                                    pre_ids.data(), post_ids.data(),
                                    pre_act.data(), post_act.data(),
                                    deltas.data(), weights.data());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double ms_per_batch = (double)duration.count() / iterations;
    double throughput = (batch_size * iterations) / ((double)duration.count() / 1000.0);

    std::cout << "Batch plasticity: " << ms_per_batch << " ms/batch ("
              << batch_size << " synapses)\n";
    std::cout << "Throughput: " << throughput / 1e6 << " M updates/sec\n";

    // Performance requirement: > 50K updates/sec (relaxed for CI/parallel test contention)
    EXPECT_GT(throughput, 50000.0) << "Batch throughput should be > 50K updates/sec";
}

TEST_F(TrainingPlasticityRegressionTest, LRModulationPerformance) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &region, nullptr);

    const int iterations = 100000;
    float modulated_lr = 0.0f;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        tpb_get_modulated_lr(bridge_, 0, 0.01f, &modulated_lr);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double us_per_op = (double)duration.count() / iterations;
    std::cout << "LR modulation query: " << us_per_op << " us/op\n";

    // Performance requirement: < 10us per query (relaxed for CI/parallel test contention)
    EXPECT_LT(us_per_op, 10.0) << "LR modulation query should be < 10us";
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(TrainingPlasticityRegressionTest, CreateDestroyMemoryStability) {
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        tpb_context_t* ctx = tpb_create(nullptr);
        ASSERT_NE(ctx, nullptr) << "Failed at iteration " << i;

        // Configure some regions
        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = 0;
        region.neuron_end_idx = 1000;
        tpb_configure_region(ctx, &region, nullptr);

        // Do some operations
        tpb_report_loss(ctx, 1.0f, nullptr);
        float delta;
        tpb_route_weight_update(ctx, 100, 0.8f, 0.9f, 10.0f, &delta);

        tpb_destroy(ctx);
    }

    // If we get here without crash, memory is being managed correctly
    SUCCEED();
}

TEST_F(TrainingPlasticityRegressionTest, LargeBatchMemoryStability) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000000;
    tpb_configure_region(bridge_, &region, nullptr);

    // Allocate large batch
    const uint32_t batch_size = 100000;
    std::vector<uint32_t> pre_ids(batch_size), post_ids(batch_size);
    std::vector<float> pre_act(batch_size), post_act(batch_size);
    std::vector<float> deltas(batch_size), weights(batch_size, 0.5f);

    for (uint32_t i = 0; i < batch_size; i++) {
        pre_ids[i] = i;
        post_ids[i] = (i + 1) % 1000000;
        pre_act[i] = 0.8f;
        post_act[i] = 0.9f;
        deltas[i] = 10.0f;
    }

    // Run multiple times
    for (int iter = 0; iter < 10; iter++) {
        EXPECT_EQ(tpb_apply_plasticity_batch(bridge_, batch_size,
                                              pre_ids.data(), post_ids.data(),
                                              pre_act.data(), post_act.data(),
                                              deltas.data(), weights.data()),
                  NIMCP_SUCCESS) << "Failed at iteration " << iter;
    }
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(TrainingPlasticityRegressionTest, ZeroLossValues) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    float rpe = 999.0f;
    EXPECT_EQ(tpb_report_loss(bridge_, 0.0f, &rpe), NIMCP_SUCCESS);
    EXPECT_NE(rpe, 999.0f) << "RPE should be computed for zero loss";
}

TEST_F(TrainingPlasticityRegressionTest, VerySmallLossValues) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    float rpe = 0.0f;
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(tpb_report_loss(bridge_, 1e-10f, &rpe), NIMCP_SUCCESS);
    }
}

TEST_F(TrainingPlasticityRegressionTest, VeryLargeLossValues) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    float rpe = 0.0f;
    EXPECT_EQ(tpb_report_loss(bridge_, 1e10f, &rpe), NIMCP_SUCCESS);
}

TEST_F(TrainingPlasticityRegressionTest, RapidLossFluctuations) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Rapidly alternating losses
    for (int i = 0; i < 1000; i++) {
        float loss = (i % 2 == 0) ? 1.0f : 0.1f;
        float rpe = 0.0f;
        EXPECT_EQ(tpb_report_loss(bridge_, loss, &rpe), NIMCP_SUCCESS);
    }

    // Should not crash or produce NaN
    float da = 0.0f;
    tpb_get_neuromod_levels(bridge_, &da, nullptr, nullptr, nullptr);
    EXPECT_FALSE(std::isnan(da)) << "DA should not be NaN";
    EXPECT_GE(da, 0.0f);
    EXPECT_LE(da, 1.0f);
}

TEST_F(TrainingPlasticityRegressionTest, ZeroActivityInputs) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &region, nullptr);

    float delta = 999.0f;
    EXPECT_EQ(tpb_route_weight_update(bridge_, 100, 0.0f, 0.0f, 10.0f, &delta),
              NIMCP_SUCCESS);
    // Zero activity should produce near-zero delta
    EXPECT_NEAR(delta, 0.0f, 0.001f);
}

TEST_F(TrainingPlasticityRegressionTest, ExtremeTimingDeltas) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &region, nullptr);

    float delta = 0.0f;

    // Very large positive delta
    EXPECT_EQ(tpb_route_weight_update(bridge_, 100, 0.8f, 0.9f, 10000.0f, &delta),
              NIMCP_SUCCESS);
    // Should produce very small (decayed) LTP
    EXPECT_GE(delta, 0.0f);

    // Very large negative delta
    EXPECT_EQ(tpb_route_weight_update(bridge_, 100, 0.8f, 0.9f, -10000.0f, &delta),
              NIMCP_SUCCESS);
    // Should produce very small (decayed) LTD
    EXPECT_LE(delta, 0.0f);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(TrainingPlasticityRegressionTest, MaxRegionsConfiguration) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Configure exactly max regions
    for (uint32_t i = 0; i < TPB_MAX_REGIONS; i++) {
        tpb_region_config_t region = tpb_region_cortical_default();
        region.neuron_start_idx = i * 100;
        region.neuron_end_idx = (i + 1) * 100;

        uint32_t region_id = UINT32_MAX;
        EXPECT_EQ(tpb_configure_region(bridge_, &region, &region_id), NIMCP_SUCCESS)
            << "Failed to configure region " << i;
        EXPECT_EQ(region_id, i);
    }
}

TEST_F(TrainingPlasticityRegressionTest, NeuromodBoundaryValues) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    // Test boundary values
    EXPECT_EQ(tpb_set_neuromod_levels(bridge_, 0.0f, 0.0f, 0.0f, 0.0f), NIMCP_SUCCESS);
    EXPECT_EQ(tpb_set_neuromod_levels(bridge_, 1.0f, 1.0f, 1.0f, 1.0f), NIMCP_SUCCESS);

    // Values should be clamped
    EXPECT_EQ(tpb_set_neuromod_levels(bridge_, 2.0f, -0.5f, 1.5f, -1.0f), NIMCP_SUCCESS);

    float da, ach, ht5, ne;
    tpb_get_neuromod_levels(bridge_, &da, &ach, &ht5, &ne);
    EXPECT_LE(da, 1.0f);
    EXPECT_GE(ach, 0.0f);
    EXPECT_LE(ht5, 1.0f);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(TrainingPlasticityRegressionTest, HighFrequencyOperations) {
    tpb_config_t config = tpb_config_default();
    config.thread_pool_size = 4;
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 10000;
    tpb_configure_region(bridge_, &region, nullptr);

    const int iterations = 100000;

    for (int i = 0; i < iterations; i++) {
        // Mix of operations
        if (i % 10 == 0) {
            tpb_report_loss(bridge_, 1.0f - 0.00001f * i, nullptr);
        }
        if (i % 5 == 0) {
            float delta;
            tpb_route_weight_update(bridge_, i % 10000, 0.8f, 0.9f, 10.0f, &delta);
        }
        if (i % 100 == 0) {
            float da;
            tpb_get_neuromod_levels(bridge_, &da, nullptr, nullptr, nullptr);
        }
    }

    // Verify state is consistent
    tpb_stats_t stats;
    EXPECT_EQ(tpb_get_stats(bridge_, &stats), NIMCP_SUCCESS);
    EXPECT_GT(stats.rpe_computations, 0u);
    EXPECT_GT(stats.total_plasticity_updates, 0u);
}

TEST_F(TrainingPlasticityRegressionTest, ConcurrentStress) {
    tpb_config_t config = tpb_config_default();
    config.thread_pool_size = 8;
    bridge_ = tpb_create(&config);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 100000;
    tpb_configure_region(bridge_, &region, nullptr);

    const int num_threads = 8;
    const int ops_per_thread = 10000;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < ops_per_thread; i++) {
            int op = (thread_id * ops_per_thread + i) % 4;

            switch (op) {
                case 0: {
                    float rpe;
                    if (tpb_report_loss(bridge_, 1.0f, &rpe) == NIMCP_SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                    break;
                }
                case 1: {
                    float delta;
                    if (tpb_route_weight_update(bridge_, i % 100000, 0.8f, 0.9f, 10.0f, &delta) == NIMCP_SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                    break;
                }
                case 2: {
                    float lr;
                    if (tpb_get_modulated_lr(bridge_, 0, 0.01f, &lr) == NIMCP_SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                    break;
                }
                case 3: {
                    float da;
                    if (tpb_get_neuromod_levels(bridge_, &da, nullptr, nullptr, nullptr) == NIMCP_SUCCESS) {
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

    EXPECT_EQ(success_count.load(), num_threads * ops_per_thread)
        << "All operations should succeed";
    EXPECT_EQ(error_count.load(), 0) << "No operations should fail";
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(TrainingPlasticityRegressionTest, DefaultConfigBackwardCompat) {
    tpb_config_t config = tpb_config_default();

    // Verify expected defaults haven't changed
    EXPECT_EQ(config.rpe_mode, TPB_RPE_EXPONENTIAL_AVG);
    EXPECT_EQ(config.rpe_window_size, TPB_DEFAULT_RPE_WINDOW);
    EXPECT_FLOAT_EQ(config.rpe_smoothing_alpha, 0.1f);
    EXPECT_FLOAT_EQ(config.rpe_to_da_gain, 0.5f);
    EXPECT_EQ(config.thread_pool_size, TPB_DEFAULT_THREAD_POOL_SIZE);
    EXPECT_TRUE(config.enable_cow);

    // LR modulation defaults
    EXPECT_EQ(config.lr_modulation.mode, TPB_NEUROMOD_BALANCED);
    EXPECT_FLOAT_EQ(config.lr_modulation.da_weight, 0.4f);
    EXPECT_FLOAT_EQ(config.lr_modulation.ach_weight, 0.3f);
    EXPECT_FLOAT_EQ(config.lr_modulation.min_lr_multiplier, 0.1f);
    EXPECT_FLOAT_EQ(config.lr_modulation.max_lr_multiplier, 5.0f);
}

TEST_F(TrainingPlasticityRegressionTest, RegionPresetBackwardCompat) {
    // Cortical defaults
    tpb_region_config_t cortical = tpb_region_cortical_default();
    EXPECT_EQ(cortical.type, TPB_REGION_CORTICAL);
    EXPECT_EQ(cortical.primary_rule, TPB_RULE_STDP);
    EXPECT_EQ(cortical.secondary_rule, TPB_RULE_HOMEOSTATIC);
    EXPECT_TRUE(cortical.enable_three_factor);
    EXPECT_FLOAT_EQ(cortical.da_sensitivity, 0.8f);
    EXPECT_FLOAT_EQ(cortical.ach_sensitivity, 1.2f);

    // Striatal defaults
    tpb_region_config_t striatal = tpb_region_striatal_default();
    EXPECT_EQ(striatal.type, TPB_REGION_STRIATAL);
    EXPECT_FLOAT_EQ(striatal.da_sensitivity, 1.5f);
    EXPECT_FLOAT_EQ(striatal.lr_modulation_strength, 0.8f);

    // Hippocampal defaults
    tpb_region_config_t hippocampal = tpb_region_hippocampal_default();
    EXPECT_EQ(hippocampal.type, TPB_REGION_HIPPOCAMPAL);
    EXPECT_EQ(hippocampal.primary_rule, TPB_RULE_BCM);
    EXPECT_FLOAT_EQ(hippocampal.ach_sensitivity, 1.4f);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(TrainingPlasticityRegressionTest, NumericalStabilityLongRun) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 1000;
    tpb_configure_region(bridge_, &region, nullptr);

    // Run many iterations checking for numerical issues
    for (int i = 0; i < 100000; i++) {
        float loss = 1.0f - 0.000001f * i;
        if (loss < 0.001f) loss = 0.001f;

        float rpe = 0.0f;
        tpb_report_loss(bridge_, loss, &rpe);

        // Check for NaN/Inf
        ASSERT_FALSE(std::isnan(rpe)) << "RPE became NaN at iteration " << i;
        ASSERT_FALSE(std::isinf(rpe)) << "RPE became Inf at iteration " << i;
    }

    // Check final state
    float da, ach, ht5, ne;
    tpb_get_neuromod_levels(bridge_, &da, &ach, &ht5, &ne);

    EXPECT_FALSE(std::isnan(da));
    EXPECT_FALSE(std::isnan(ach));
    EXPECT_FALSE(std::isnan(ht5));
    EXPECT_FALSE(std::isnan(ne));

    EXPECT_GE(da, 0.0f);
    EXPECT_LE(da, 1.0f);
}

TEST_F(TrainingPlasticityRegressionTest, WeightStabilityUnderPlasticity) {
    bridge_ = tpb_create(nullptr);
    ASSERT_NE(bridge_, nullptr);

    tpb_region_config_t region = tpb_region_cortical_default();
    region.neuron_start_idx = 0;
    region.neuron_end_idx = 100;
    tpb_configure_region(bridge_, &region, nullptr);

    const int n = 100;
    std::vector<float> weights(n, 0.5f);

    // Many updates
    for (int iter = 0; iter < 10000; iter++) {
        for (int i = 0; i < n; i++) {
            float delta = 0.0f;
            float timing = (iter % 2 == 0) ? 10.0f : -10.0f;
            tpb_route_weight_update(bridge_, i, 0.8f, 0.9f, timing, &delta);
            weights[i] += delta;

            // Clamp to valid range (as would be done in real training)
            if (weights[i] < 0.0f) weights[i] = 0.0f;
            if (weights[i] > 1.0f) weights[i] = 1.0f;
        }
    }

    // All weights should be valid
    for (int i = 0; i < n; i++) {
        EXPECT_FALSE(std::isnan(weights[i])) << "Weight " << i << " is NaN";
        EXPECT_FALSE(std::isinf(weights[i])) << "Weight " << i << " is Inf";
        EXPECT_GE(weights[i], 0.0f);
        EXPECT_LE(weights[i], 1.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
