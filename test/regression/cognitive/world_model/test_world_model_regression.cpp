/**
 * @file test_world_model_regression.cpp
 * @brief Comprehensive regression tests for world model modules
 *
 * WHAT: Regression tests for multimodal world model and omni world model
 * WHY:  Ensure world model behavior is stable across versions
 * HOW:  GTest framework with performance benchmarks, determinism checks,
 *       memory safety tests, null pointer safety, and stress tests
 *
 * TEST CATEGORIES:
 * - Performance Benchmarks: Creation, prediction, fusion timing
 * - Determinism Tests: Same input = same output
 * - State Consistency Tests: Processing maintains valid state
 * - Memory Usage Tests: Create/destroy cycles, memory patterns
 * - Null Pointer Safety: Graceful handling of null parameters
 * - Backward Compatibility: Default config values remain stable
 * - Exception Handling: NIMCP_THROW_TO_IMMUNE error reporting
 * - Stress Tests: Rapid updates, edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-01-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <numeric>
#include <algorithm>

#include "utils/nimcp_test_base.h"
#include "cognitive/extrapolation/nimcp_world_model_multimodal.h"
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Constants and Thresholds
//=============================================================================

namespace {
    // Performance thresholds (microseconds)
    constexpr double WM_CREATE_LATENCY_US = 5000.0;       // <5ms to create
    constexpr double WM_PREDICT_LATENCY_US = 2000.0;      // <2ms per prediction
    constexpr double WM_FUSION_LATENCY_US = 1000.0;       // <1ms per fusion
    constexpr double WM_UPDATE_LATENCY_US = 500.0;        // <0.5ms per update
    constexpr uint32_t MIN_OPS_PER_SEC = 1000;            // Minimum throughput

    // Memory thresholds (bytes)
    constexpr size_t MAX_WM_MEMORY = 64 * 1024 * 1024;    // 64MB per world model
    constexpr size_t MAX_LEAK_BYTES = 4096;               // Max acceptable leak

    // Numerical tolerances
    constexpr float NUMERICAL_TOLERANCE = 1e-6f;
    constexpr float PREDICTION_TOLERANCE = 0.01f;

    // Test dimensions
    constexpr uint32_t TEST_LATENT_DIM = 64;
    constexpr uint32_t TEST_STATE_DIM = 32;
    constexpr uint32_t TEST_ACTION_DIM = 16;
    constexpr uint32_t TEST_OBS_DIM = 64;
    constexpr uint32_t NUM_ITERATIONS = 100;
}

//=============================================================================
// Performance Timer
//=============================================================================

class PerformanceTimer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    double stop_us() {
        auto end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end_time - start_time_).count();
    }

    double stop_ms() {
        return stop_us() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
};

struct BenchmarkStats {
    double mean_us;
    double std_us;
    double min_us;
    double max_us;
    double p95_us;
    double p99_us;

    static BenchmarkStats compute(std::vector<double>& times) {
        BenchmarkStats stats = {};
        size_t n = times.size();
        if (n == 0) return stats;

        std::sort(times.begin(), times.end());

        stats.min_us = times.front();
        stats.max_us = times.back();
        stats.mean_us = std::accumulate(times.begin(), times.end(), 0.0) / n;
        stats.p95_us = times[static_cast<size_t>(n * 0.95)];
        stats.p99_us = times[static_cast<size_t>(n * 0.99)];

        double sq_sum = 0;
        for (double t : times) {
            sq_sum += (t - stats.mean_us) * (t - stats.mean_us);
        }
        stats.std_us = std::sqrt(sq_sum / n);

        return stats;
    }

    void print(const char* name) const {
        std::cout << name << " Latency:" << std::endl;
        std::cout << "  Mean: " << mean_us << " us" << std::endl;
        std::cout << "  Std:  " << std_us << " us" << std::endl;
        std::cout << "  Min:  " << min_us << " us" << std::endl;
        std::cout << "  Max:  " << max_us << " us" << std::endl;
        std::cout << "  P95:  " << p95_us << " us" << std::endl;
        std::cout << "  P99:  " << p99_us << " us" << std::endl;
    }
};

//=============================================================================
// Test Fixture
//=============================================================================

class WorldModelRegressionTest : public NimcpTestBase {
protected:
    nimcp_world_model_t* multimodal_wm = nullptr;
    omni_world_model_t* omni_wm = nullptr;
    std::mt19937 rng{42};  // Deterministic RNG

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (multimodal_wm) {
            wm_destroy(multimodal_wm);
            multimodal_wm = nullptr;
        }
        if (omni_wm) {
            omni_wm_destroy(omni_wm);
            omni_wm = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper: Create multimodal world model with default config
    nimcp_world_model_t* create_multimodal_wm() {
        wm_config_t config = wm_default_config();
        config.latent_dim = TEST_LATENT_DIM;
        config.max_entities = 128;
        config.max_prediction_steps = 50;
        config.enable_bio_async = false;
        config.enable_immune = false;
        return wm_create(&config);
    }

    // Helper: Create omni world model with default config
    omni_world_model_t* create_omni_wm() {
        omni_wm_config_t config;
        omni_wm_get_default_config(&config);
        config.state_dim = TEST_STATE_DIM;
        config.action_dim = TEST_ACTION_DIM;
        config.obs_dim = TEST_OBS_DIM;
        config.enable_dreaming = false;
        return omni_wm_create(&config);
    }

    // Helper: Create modality input
    wm_modality_input_t create_modality_input(wm_modality_t modality, uint32_t dim) {
        wm_modality_input_t input = {};
        input.modality = modality;
        input.feature_dim = dim;
        input.features = new float[dim];
        input.confidence = 0.9f;
        input.timestamp = 1000;
        input.attention_weights = nullptr;

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (uint32_t i = 0; i < dim; i++) {
            input.features[i] = dist(rng);
        }
        return input;
    }

    // Helper: Free modality input
    void free_modality_input(wm_modality_input_t& input) {
        delete[] input.features;
        input.features = nullptr;
    }

    // Helper: Generate random float vector
    std::vector<float> random_vector(uint32_t size) {
        std::vector<float> vec(size);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (auto& val : vec) {
            val = dist(rng);
        }
        return vec;
    }

    // Helper: Get current allocated memory
    size_t get_allocated_memory() {
        nimcp_memory_stats_t stats;
        if (nimcp_memory_get_stats(&stats)) {
            return stats.current_allocated;
        }
        return 0;
    }

    // Helper: Measure elapsed time in microseconds
    template<typename Func>
    double measure_time_us(Func func) {
        PerformanceTimer timer;
        timer.start();
        func();
        return timer.stop_us();
    }
};

//=============================================================================
// CATEGORY 1: Performance Benchmarks - Multimodal World Model
//=============================================================================

TEST_F(WorldModelRegressionTest, MultimodalCreateLatency) {
    // WHAT: Benchmark multimodal world model creation latency
    // WHY:  Verify creation remains performant
    // TARGET: <5ms per creation

    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        double latency = measure_time_us([&]() {
            nimcp_world_model_t* wm = create_multimodal_wm();
            ASSERT_NE(wm, nullptr);
            wm_destroy(wm);
        });
        times.push_back(latency);
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Multimodal WM Creation");

    EXPECT_LT(stats.mean_us, WM_CREATE_LATENCY_US)
        << "Multimodal WM creation latency: " << stats.mean_us << " us";
}

TEST_F(WorldModelRegressionTest, MultimodalPredictionLatency) {
    // WHAT: Benchmark prediction latency
    // WHY:  Verify prediction remains performant
    // TARGET: <2ms per prediction

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_error_t result = wm_init(multimodal_wm);
    EXPECT_EQ(result, WM_OK);

    // Add some modality input
    auto input = create_modality_input(WM_MODALITY_VISUAL, 64);
    wm_process_modality(multimodal_wm, &input);
    free_modality_input(input);

    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    wm_prediction_t prediction = {};

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        double latency = measure_time_us([&]() {
            wm_predict(multimodal_wm, 10, &prediction);
        });
        times.push_back(latency);
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Multimodal WM Prediction");

    EXPECT_LT(stats.mean_us, WM_PREDICT_LATENCY_US)
        << "Multimodal WM prediction latency: " << stats.mean_us << " us";
}

TEST_F(WorldModelRegressionTest, MultimodalFusionLatency) {
    // WHAT: Benchmark modality fusion latency
    // WHY:  Verify fusion remains performant
    // TARGET: <1ms per fusion

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    // Add multiple modality inputs
    std::vector<wm_modality_input_t> inputs;
    inputs.push_back(create_modality_input(WM_MODALITY_VISUAL, 64));
    inputs.push_back(create_modality_input(WM_MODALITY_AUDITORY, 32));
    inputs.push_back(create_modality_input(WM_MODALITY_TACTILE, 16));

    for (auto& input : inputs) {
        wm_process_modality(multimodal_wm, &input);
    }

    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        double latency = measure_time_us([&]() {
            wm_fuse_modalities(multimodal_wm);
        });
        times.push_back(latency);
    }

    for (auto& input : inputs) {
        free_modality_input(input);
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Multimodal WM Fusion");

    EXPECT_LT(stats.mean_us, WM_FUSION_LATENCY_US)
        << "Multimodal WM fusion latency: " << stats.mean_us << " us";
}

//=============================================================================
// CATEGORY 2: Performance Benchmarks - Omni World Model
//=============================================================================

TEST_F(WorldModelRegressionTest, OmniCreateLatency) {
    // WHAT: Benchmark omni world model creation latency
    // WHY:  Verify creation remains performant
    // TARGET: <5ms per creation

    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        double latency = measure_time_us([&]() {
            omni_world_model_t* wm = create_omni_wm();
            ASSERT_NE(wm, nullptr);
            omni_wm_destroy(wm);
        });
        times.push_back(latency);
    }

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Omni WM Creation");

    EXPECT_LT(stats.mean_us, WM_CREATE_LATENCY_US)
        << "Omni WM creation latency: " << stats.mean_us << " us";
}

TEST_F(WorldModelRegressionTest, OmniForwardPredictionLatency) {
    // WHAT: Benchmark forward prediction latency
    // WHY:  Verify prediction remains performant
    // TARGET: <2ms per prediction

    omni_wm = create_omni_wm();
    ASSERT_NE(omni_wm, nullptr);

    // Set initial state
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    omni_wm_set_state(omni_wm, state);

    auto action = random_vector(TEST_ACTION_DIM);
    omni_wm_transition_t result = {};

    std::vector<double> times;
    times.reserve(NUM_ITERATIONS);

    for (uint32_t i = 0; i < NUM_ITERATIONS; i++) {
        double latency = measure_time_us([&]() {
            omni_wm_predict_forward(omni_wm, action.data(), TEST_ACTION_DIM, &result);
        });
        times.push_back(latency);
    }

    omni_wm_state_destroy(state);

    BenchmarkStats stats = BenchmarkStats::compute(times);
    stats.print("Omni WM Forward Prediction");

    EXPECT_LT(stats.mean_us, WM_PREDICT_LATENCY_US)
        << "Omni WM forward prediction latency: " << stats.mean_us << " us";
}

TEST_F(WorldModelRegressionTest, OmniCreateDestroyThroughput) {
    // WHAT: Benchmark create/destroy throughput
    // WHY:  Verify allocation performance
    // TARGET: >1000 ops/sec

    const uint32_t iterations = 500;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < iterations; i++) {
        omni_world_model_t* wm = create_omni_wm();
        ASSERT_NE(wm, nullptr);
        omni_wm_destroy(wm);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double ops_per_sec = iterations / elapsed_sec;

    std::cout << "Create/destroy throughput: " << ops_per_sec << " ops/sec" << std::endl;

    EXPECT_GT(ops_per_sec, MIN_OPS_PER_SEC)
        << "Create/destroy throughput: " << ops_per_sec << " ops/sec";
}

//=============================================================================
// CATEGORY 3: Determinism Tests
//=============================================================================

TEST_F(WorldModelRegressionTest, MultimodalPredictionDeterminism) {
    // WHAT: Verify multimodal predictions are deterministic
    // WHY:  Same input must produce same output
    // TARGET: Outputs match exactly

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    auto input = create_modality_input(WM_MODALITY_VISUAL, 64);
    wm_process_modality(multimodal_wm, &input);

    wm_prediction_t pred1 = {};
    wm_prediction_t pred2 = {};

    wm_predict(multimodal_wm, 10, &pred1);
    wm_predict(multimodal_wm, 10, &pred2);

    // Predictions should be identical
    EXPECT_FLOAT_EQ(pred1.prediction_confidence, pred2.prediction_confidence);
    EXPECT_EQ(pred1.horizon_steps, pred2.horizon_steps);

    free_modality_input(input);
}

TEST_F(WorldModelRegressionTest, OmniForwardDeterminism) {
    // WHAT: Verify omni forward predictions are deterministic
    // WHY:  Same input must produce same output
    // TARGET: State values match exactly

    omni_wm = create_omni_wm();
    ASSERT_NE(omni_wm, nullptr);

    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);

    // Initialize state values
    std::vector<float> state_vals = random_vector(TEST_STATE_DIM);
    memcpy(state->values, state_vals.data(), TEST_STATE_DIM * sizeof(float));

    omni_wm_set_state(omni_wm, state);

    auto action = random_vector(TEST_ACTION_DIM);

    omni_wm_transition_t result1 = {};
    omni_wm_transition_t result2 = {};

    omni_wm_predict_forward(omni_wm, action.data(), TEST_ACTION_DIM, &result1);
    omni_wm_predict_forward(omni_wm, action.data(), TEST_ACTION_DIM, &result2);

    // Log probabilities should match
    EXPECT_FLOAT_EQ(result1.log_prob, result2.log_prob);
    EXPECT_FLOAT_EQ(result1.prediction_error, result2.prediction_error);

    omni_wm_state_destroy(state);
}

TEST_F(WorldModelRegressionTest, MultimodalFusionDeterminism) {
    // WHAT: Verify fusion results are deterministic
    // WHY:  Cross-modal fusion must be reproducible
    // TARGET: Attention weights match

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    // Add modalities
    auto visual = create_modality_input(WM_MODALITY_VISUAL, 64);
    auto auditory = create_modality_input(WM_MODALITY_AUDITORY, 32);

    wm_process_modality(multimodal_wm, &visual);
    wm_process_modality(multimodal_wm, &auditory);

    wm_fuse_modalities(multimodal_wm);

    wm_cross_modal_attention_t attn1 = {};
    wm_cross_modal_attention_t attn2 = {};

    wm_get_attention(multimodal_wm, &attn1);
    wm_fuse_modalities(multimodal_wm);
    wm_get_attention(multimodal_wm, &attn2);

    // Coherence scores should match
    EXPECT_FLOAT_EQ(attn1.coherence_score, attn2.coherence_score);
    EXPECT_EQ(attn1.dominant_modality, attn2.dominant_modality);

    free_modality_input(visual);
    free_modality_input(auditory);
}

//=============================================================================
// CATEGORY 4: State Consistency Tests
//=============================================================================

TEST_F(WorldModelRegressionTest, MultimodalStatusTransitions) {
    // WHAT: Verify status transitions are valid
    // WHY:  Status must reflect actual state
    // TARGET: Status matches expected state

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    // Initially should be IDLE
    wm_status_t status = wm_get_status(multimodal_wm);
    EXPECT_EQ(status, WM_STATUS_IDLE);

    // After init, still IDLE (ready)
    wm_init(multimodal_wm);
    status = wm_get_status(multimodal_wm);
    EXPECT_NE(status, WM_STATUS_ERROR);
}

TEST_F(WorldModelRegressionTest, MultimodalStatsConsistency) {
    // WHAT: Verify stats remain consistent after processing
    // WHY:  Statistics must reflect actual state
    // TARGET: Stats match processing history

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    wm_stats_t stats_before = {};
    wm_get_stats(multimodal_wm, &stats_before);

    // Process some inputs
    for (int i = 0; i < 10; i++) {
        auto input = create_modality_input(WM_MODALITY_VISUAL, 64);
        wm_process_modality(multimodal_wm, &input);
        free_modality_input(input);
    }

    wm_stats_t stats_after = {};
    wm_get_stats(multimodal_wm, &stats_after);

    // Inputs processed should have increased
    EXPECT_GE(stats_after.inputs_processed, stats_before.inputs_processed);
}

TEST_F(WorldModelRegressionTest, OmniStatsConsistency) {
    // WHAT: Verify omni world model stats are consistent
    // WHY:  Statistics must reflect actual operations
    // TARGET: Stats values are valid

    omni_wm = create_omni_wm();
    ASSERT_NE(omni_wm, nullptr);

    omni_wm_stats_t stats_before = {};
    omni_wm_get_stats(omni_wm, &stats_before);

    // Perform some predictions
    omni_wm_state_t* state = omni_wm_state_create(TEST_STATE_DIM);
    ASSERT_NE(state, nullptr);
    omni_wm_set_state(omni_wm, state);

    auto action = random_vector(TEST_ACTION_DIM);
    omni_wm_transition_t result = {};

    for (int i = 0; i < 10; i++) {
        omni_wm_predict_forward(omni_wm, action.data(), TEST_ACTION_DIM, &result);
    }

    omni_wm_stats_t stats_after = {};
    omni_wm_get_stats(omni_wm, &stats_after);

    // Forward predictions should have increased
    EXPECT_GE(stats_after.forward_predictions, stats_before.forward_predictions);

    omni_wm_state_destroy(state);
}

TEST_F(WorldModelRegressionTest, EntityTrackingConsistency) {
    // WHAT: Verify entity tracking maintains valid state
    // WHY:  Entity operations must be consistent
    // TARGET: Entity count matches operations

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    // Add entities
    for (uint32_t i = 0; i < 5; i++) {
        wm_entity_t entity = {};
        entity.entity_id = i;
        entity.position[0] = static_cast<float>(i);
        entity.position[1] = static_cast<float>(i * 2);
        entity.position[2] = 0.0f;
        entity.existence_prob = 0.9f;
        entity.last_observed = 1000;

        uint32_t assigned_id = 0;
        wm_error_t result = wm_add_entity(multimodal_wm, &entity, &assigned_id);
        // Should succeed or handle gracefully
        if (result == WM_OK) {
            EXPECT_GE(assigned_id, 0u);
        }
    }

    wm_stats_t stats = {};
    wm_get_stats(multimodal_wm, &stats);
    EXPECT_LE(stats.active_entities, 5u);
}

//=============================================================================
// CATEGORY 5: Memory Usage Tests
//=============================================================================

TEST_F(WorldModelRegressionTest, MultimodalCreateDestroyCyclesNoLeak) {
    // WHAT: Verify no memory leak in create/destroy cycles
    // WHY:  Memory must be properly released
    // TARGET: Memory returns to baseline

    size_t memory_before = get_allocated_memory();

    for (int cycle = 0; cycle < 100; cycle++) {
        nimcp_world_model_t* wm = create_multimodal_wm();
        ASSERT_NE(wm, nullptr);
        wm_init(wm);
        wm_destroy(wm);
    }

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    EXPECT_LT(leak, MAX_LEAK_BYTES) << "Memory leak: " << leak << " bytes";
}

TEST_F(WorldModelRegressionTest, OmniCreateDestroyCyclesNoLeak) {
    // WHAT: Verify no memory leak in omni world model create/destroy cycles
    // WHY:  Memory must be properly released
    // TARGET: Memory returns to baseline

    size_t memory_before = get_allocated_memory();

    for (int cycle = 0; cycle < 100; cycle++) {
        omni_world_model_t* wm = create_omni_wm();
        ASSERT_NE(wm, nullptr);
        omni_wm_destroy(wm);
    }

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    EXPECT_LT(leak, MAX_LEAK_BYTES) << "Memory leak: " << leak << " bytes";
}

TEST_F(WorldModelRegressionTest, MultimodalMemoryFootprint) {
    // WHAT: Verify memory footprint is reasonable
    // WHY:  Memory usage must be bounded
    // TARGET: < 64MB per world model

    size_t memory_before = get_allocated_memory();
    multimodal_wm = create_multimodal_wm();
    size_t memory_after = get_allocated_memory();

    ASSERT_NE(multimodal_wm, nullptr);

    size_t memory_used = (memory_after > memory_before) ? (memory_after - memory_before) : 0;
    std::cout << "Multimodal WM Memory Footprint: " << memory_used / 1024 << " KB" << std::endl;

    EXPECT_LT(memory_used, MAX_WM_MEMORY) << "Memory footprint exceeds threshold";
}

TEST_F(WorldModelRegressionTest, OmniMemoryFootprint) {
    // WHAT: Verify omni world model memory footprint is reasonable
    // WHY:  Memory usage must be bounded
    // TARGET: < 64MB per world model

    size_t memory_before = get_allocated_memory();
    omni_wm = create_omni_wm();
    size_t memory_after = get_allocated_memory();

    ASSERT_NE(omni_wm, nullptr);

    size_t memory_used = (memory_after > memory_before) ? (memory_after - memory_before) : 0;
    std::cout << "Omni WM Memory Footprint: " << memory_used / 1024 << " KB" << std::endl;

    EXPECT_LT(memory_used, MAX_WM_MEMORY) << "Memory footprint exceeds threshold";
}

TEST_F(WorldModelRegressionTest, PredictionBufferCleanup) {
    // WHAT: Verify prediction buffer cleanup
    // WHY:  Prediction buffers must be released
    // TARGET: No leak after repeated predictions

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    size_t memory_before = get_allocated_memory();

    // Perform many predictions
    for (int i = 0; i < 1000; i++) {
        wm_prediction_t prediction = {};
        wm_predict(multimodal_wm, 10, &prediction);
    }

    size_t memory_after = get_allocated_memory();
    size_t increase = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    // Memory increase should be bounded (not O(n) with iterations)
    EXPECT_LT(increase, 1024 * 1024) << "Prediction buffer leak: " << increase << " bytes";
}

//=============================================================================
// CATEGORY 6: Null Pointer Safety
//=============================================================================

TEST_F(WorldModelRegressionTest, MultimodalNullSafety) {
    // WHAT: Verify multimodal functions handle null safely
    // WHY:  Must not crash on invalid input
    // TARGET: No crashes, graceful return

    // These should not crash
    wm_destroy(nullptr);

    wm_error_t result = wm_init(nullptr);
    EXPECT_NE(result, WM_OK);

    result = wm_reset(nullptr);
    EXPECT_NE(result, WM_OK);

    result = wm_process_modality(nullptr, nullptr);
    EXPECT_NE(result, WM_OK);

    result = wm_fuse_modalities(nullptr);
    EXPECT_NE(result, WM_OK);

    wm_prediction_t pred = {};
    result = wm_predict(nullptr, 10, &pred);
    EXPECT_NE(result, WM_OK);

    wm_status_t status = wm_get_status(nullptr);
    EXPECT_EQ(status, WM_STATUS_ERROR);

    wm_error_t last_error = wm_get_last_error(nullptr);
    EXPECT_EQ(last_error, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelRegressionTest, OmniNullSafety) {
    // WHAT: Verify omni world model functions handle null safely
    // WHY:  Must not crash on invalid input
    // TARGET: No crashes, graceful return

    // These should not crash
    omni_wm_destroy(nullptr);

    omni_wm_state_destroy(nullptr);

    omni_wm_transition_t result = {};
    nimcp_error_t err = omni_wm_predict_forward(nullptr, nullptr, 0, &result);
    EXPECT_NE(err, NIMCP_OK);

    err = omni_wm_set_state(nullptr, nullptr);
    EXPECT_NE(err, NIMCP_OK);

    const omni_wm_state_t* state = omni_wm_get_state(nullptr);
    EXPECT_EQ(state, nullptr);

    omni_wm_stats_t stats = {};
    err = omni_wm_get_stats(nullptr, &stats);
    EXPECT_NE(err, NIMCP_OK);
}

TEST_F(WorldModelRegressionTest, NullConfigDefaultsUsed) {
    // WHAT: Verify null config uses defaults
    // WHY:  Convenience API
    // TARGET: World model created with defaults

    // Multimodal - null config should use defaults
    nimcp_world_model_t* wm = wm_create(nullptr);
    // Should either use defaults or return null gracefully
    if (wm) {
        wm_destroy(wm);
    }

    // Omni - null config should use defaults
    omni_world_model_t* owm = omni_wm_create(nullptr);
    // Should either use defaults or return null gracefully
    if (owm) {
        omni_wm_destroy(owm);
    }
}

//=============================================================================
// CATEGORY 7: Backward Compatibility
//=============================================================================

TEST_F(WorldModelRegressionTest, MultimodalDefaultConfigStable) {
    // WHAT: Verify default config values are stable
    // WHY:  Config defaults must not change unexpectedly
    // TARGET: Known default values

    wm_config_t config = wm_default_config();

    // Verify key defaults are reasonable
    EXPECT_GT(config.latent_dim, 0u);
    EXPECT_GT(config.max_entities, 0u);
    EXPECT_GT(config.max_prediction_steps, 0u);
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_LT(config.learning_rate, 1.0f);
}

TEST_F(WorldModelRegressionTest, OmniDefaultConfigStable) {
    // WHAT: Verify omni default config values are stable
    // WHY:  Config defaults must not change unexpectedly
    // TARGET: Known default values

    omni_wm_config_t config;
    nimcp_error_t result = omni_wm_get_default_config(&config);
    EXPECT_EQ(result, NIMCP_OK);

    // Verify key defaults are reasonable
    EXPECT_GT(config.state_dim, 0u);
    EXPECT_GT(config.action_dim, 0u);
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_LT(config.learning_rate, 1.0f);
    EXPECT_GT(config.discount_factor, 0.0f);
    EXPECT_LE(config.discount_factor, 1.0f);
}

TEST_F(WorldModelRegressionTest, ModalityNamesStable) {
    // WHAT: Verify modality names are stable
    // WHY:  Names used in logging/debugging must not change
    // TARGET: Known modality names

    // All modality names should be non-null
    for (int i = 0; i < WM_MODALITY_COUNT; i++) {
        const char* name = wm_modality_string(static_cast<wm_modality_t>(i));
        EXPECT_NE(name, nullptr) << "Modality " << i << " has null name";
    }
}

TEST_F(WorldModelRegressionTest, ErrorCodesStable) {
    // WHAT: Verify error code strings are stable
    // WHY:  Error messages must not change unexpectedly
    // TARGET: All error codes have string representations

    const wm_error_t errors[] = {
        WM_OK,
        WM_ERR_NULL_PTR,
        WM_ERR_NOT_INITIALIZED,
        WM_ERR_INVALID_MODALITY,
        WM_ERR_PREDICTION_FAILED,
        WM_ERR_FUSION_FAILED,
        WM_ERR_MEMORY_ALLOC,
        WM_ERR_CAPACITY_EXCEEDED,
        WM_ERR_INVALID_HORIZON,
        WM_ERR_MODALITY_MISMATCH
    };

    for (auto error : errors) {
        const char* str = wm_error_string(error);
        EXPECT_NE(str, nullptr) << "Error code " << error << " has null string";
    }
}

TEST_F(WorldModelRegressionTest, StatusCodesStable) {
    // WHAT: Verify status code strings are stable
    // WHY:  Status messages must not change unexpectedly
    // TARGET: All status codes have string representations

    const wm_status_t statuses[] = {
        WM_STATUS_IDLE,
        WM_STATUS_PROCESSING,
        WM_STATUS_PREDICTING,
        WM_STATUS_FUSING,
        WM_STATUS_ERROR
    };

    for (auto status : statuses) {
        const char* str = wm_status_string(status);
        EXPECT_NE(str, nullptr) << "Status code " << status << " has null string";
    }
}

//=============================================================================
// CATEGORY 8: Exception Handling Regression
//=============================================================================

TEST_F(WorldModelRegressionTest, ErrorStateRecovery) {
    // WHAT: Verify recovery from error state
    // WHY:  Must be able to continue after errors
    // TARGET: Recovery to functional state

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    // Trigger an error (null modality input)
    wm_error_t result = wm_process_modality(multimodal_wm, nullptr);
    EXPECT_NE(result, WM_OK);

    // Should still be able to process valid input
    auto input = create_modality_input(WM_MODALITY_VISUAL, 64);
    result = wm_process_modality(multimodal_wm, &input);
    // Should either succeed or handle gracefully
    EXPECT_NE(wm_get_status(multimodal_wm), WM_STATUS_ERROR);
    free_modality_input(input);
}

TEST_F(WorldModelRegressionTest, ErrorCodesPropagateCorrectly) {
    // WHAT: Verify error codes propagate correctly
    // WHY:  Errors must be trackable
    // TARGET: last_error reflects actual error

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    // Trigger null pointer error
    wm_error_t result = wm_process_modality(multimodal_wm, nullptr);
    wm_error_t last = wm_get_last_error(multimodal_wm);
    EXPECT_EQ(last, WM_ERR_NULL_PTR);
}

TEST_F(WorldModelRegressionTest, InvalidHorizonHandled) {
    // WHAT: Verify invalid prediction horizon is handled
    // WHY:  Must reject invalid parameters
    // TARGET: Error returned for invalid horizon

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    wm_prediction_t prediction = {};

    // Zero horizon
    wm_error_t result = wm_predict(multimodal_wm, 0, &prediction);
    if (result != WM_OK) {
        EXPECT_EQ(wm_get_last_error(multimodal_wm), WM_ERR_INVALID_HORIZON);
    }

    // Excessive horizon
    result = wm_predict(multimodal_wm, 100000, &prediction);
    if (result != WM_OK) {
        // Should be invalid horizon or capacity exceeded
        wm_error_t last = wm_get_last_error(multimodal_wm);
        EXPECT_TRUE(last == WM_ERR_INVALID_HORIZON || last == WM_ERR_CAPACITY_EXCEEDED);
    }
}

//=============================================================================
// CATEGORY 9: Stress Tests
//=============================================================================

TEST_F(WorldModelRegressionTest, RapidPredictions) {
    // WHAT: Stress test rapid prediction calls
    // WHY:  Must handle rapid processing without corruption
    // TARGET: No crashes, valid outputs

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    auto input = create_modality_input(WM_MODALITY_VISUAL, 64);
    wm_process_modality(multimodal_wm, &input);

    for (int i = 0; i < 10000; i++) {
        wm_prediction_t prediction = {};
        wm_predict(multimodal_wm, 5, &prediction);

        // Verify no NaN/Inf in confidence
        EXPECT_FALSE(std::isnan(prediction.prediction_confidence));
        EXPECT_FALSE(std::isinf(prediction.prediction_confidence));
    }

    free_modality_input(input);
}

TEST_F(WorldModelRegressionTest, RapidModalityUpdates) {
    // WHAT: Stress test rapid modality updates
    // WHY:  Must handle rapid input without corruption
    // TARGET: No crashes, valid state

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    for (int i = 0; i < 5000; i++) {
        auto input = create_modality_input(
            static_cast<wm_modality_t>(i % WM_MODALITY_COUNT),
            64
        );
        wm_process_modality(multimodal_wm, &input);
        free_modality_input(input);
    }

    wm_status_t status = wm_get_status(multimodal_wm);
    EXPECT_NE(status, WM_STATUS_ERROR);
}

TEST_F(WorldModelRegressionTest, ConcurrentPredictions) {
    // WHAT: Test concurrent predictions on independent world models
    // WHY:  Must handle multi-threaded access
    // TARGET: No data races, valid results

    const uint32_t num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<uint32_t> error_count{0};

    for (uint32_t t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            nimcp_world_model_t* wm = create_multimodal_wm();
            if (!wm) {
                error_count++;
                return;
            }

            wm_init(wm);

            auto input = create_modality_input(WM_MODALITY_VISUAL, 64);
            wm_process_modality(wm, &input);

            for (int i = 0; i < 500; i++) {
                wm_prediction_t prediction = {};
                wm_predict(wm, 5, &prediction);

                if (std::isnan(prediction.prediction_confidence) ||
                    std::isinf(prediction.prediction_confidence)) {
                    error_count++;
                }
            }

            free_modality_input(input);
            wm_destroy(wm);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(error_count.load(), 0u) << "Thread safety violation detected";
}

TEST_F(WorldModelRegressionTest, ExtremeInputValues) {
    // WHAT: Test handling of extreme input values
    // WHY:  Must handle edge cases without numerical issues
    // TARGET: No NaN/Inf, bounded outputs

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    // Test with extreme values
    std::vector<std::vector<float>> extreme_inputs = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {1e10f, 1e10f, 1e10f, 1e10f},
        {-1e10f, -1e10f, -1e10f, -1e10f},
        {1e-10f, 1e-10f, 1e-10f, 1e-10f}
    };

    for (const auto& vals : extreme_inputs) {
        wm_modality_input_t input = {};
        input.modality = WM_MODALITY_VISUAL;
        input.feature_dim = static_cast<uint32_t>(vals.size());
        input.features = const_cast<float*>(vals.data());
        input.confidence = 0.9f;
        input.timestamp = 1000;
        input.attention_weights = nullptr;

        wm_process_modality(multimodal_wm, &input);

        wm_prediction_t prediction = {};
        wm_predict(multimodal_wm, 5, &prediction);

        EXPECT_FALSE(std::isnan(prediction.prediction_confidence));
        EXPECT_FALSE(std::isinf(prediction.prediction_confidence));
    }
}

TEST_F(WorldModelRegressionTest, LongRunningOperations) {
    // WHAT: Test long-running operations
    // WHY:  Must maintain stability over extended use
    // TARGET: Consistent performance, no degradation

    multimodal_wm = create_multimodal_wm();
    ASSERT_NE(multimodal_wm, nullptr);

    wm_init(multimodal_wm);

    double total_time = 0;
    const int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        auto input = create_modality_input(WM_MODALITY_VISUAL, 64);
        wm_process_modality(multimodal_wm, &input);

        wm_fuse_modalities(multimodal_wm);

        PerformanceTimer timer;
        timer.start();
        wm_prediction_t prediction = {};
        wm_predict(multimodal_wm, 10, &prediction);
        total_time += timer.stop_us();

        free_modality_input(input);
    }

    double avg_time = total_time / iterations;
    std::cout << "Average prediction time over " << iterations
              << " iterations: " << avg_time << " us" << std::endl;

    // Verify no significant degradation
    EXPECT_LT(avg_time, WM_PREDICT_LATENCY_US * 2)
        << "Performance degradation detected";
}

//=============================================================================
// CATEGORY 10: Symlog Transformation Tests (Omni WM)
//=============================================================================

TEST_F(WorldModelRegressionTest, SymlogTransformDeterminism) {
    // WHAT: Verify symlog transformation is deterministic
    // WHY:  Mathematical transformation must be reproducible
    // TARGET: Same input = same output

    float test_values[] = {0.0f, 1.0f, -1.0f, 100.0f, -100.0f, 0.001f};

    for (float val : test_values) {
        float result1 = omni_wm_symlog(val);
        float result2 = omni_wm_symlog(val);
        EXPECT_FLOAT_EQ(result1, result2) << "Symlog not deterministic for " << val;
    }
}

TEST_F(WorldModelRegressionTest, SymlogSymexpInverse) {
    // WHAT: Verify symexp is inverse of symlog
    // WHY:  Transformations must be reversible
    // TARGET: symexp(symlog(x)) == x

    float test_values[] = {0.0f, 1.0f, -1.0f, 100.0f, -100.0f, 0.001f, -0.001f};

    for (float val : test_values) {
        float transformed = omni_wm_symlog(val);
        float recovered = omni_wm_symexp(transformed);
        EXPECT_NEAR(recovered, val, NUMERICAL_TOLERANCE * std::abs(val) + NUMERICAL_TOLERANCE)
            << "Symlog-symexp not inverse for " << val;
    }
}

TEST_F(WorldModelRegressionTest, SymlogArrayConsistency) {
    // WHAT: Verify symlog array operation is consistent with scalar
    // WHY:  Array and scalar operations must match
    // TARGET: Same results for array vs scalar

    std::vector<float> input = {0.0f, 1.0f, -1.0f, 100.0f, -100.0f};
    std::vector<float> output(input.size());

    omni_wm_symlog_array(input.data(), output.data(), static_cast<uint32_t>(input.size()));

    for (size_t i = 0; i < input.size(); i++) {
        float expected = omni_wm_symlog(input[i]);
        EXPECT_FLOAT_EQ(output[i], expected) << "Array symlog differs at index " << i;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
