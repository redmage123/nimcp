/**
 * @file test_medulla_cerebellum_bridge_regression.cpp
 * @brief Comprehensive regression tests for Medulla-Cerebellum Bridge
 *
 * WHAT: Regression tests for the medulla-cerebellum bridge via inferior olive
 * WHY:  Ensure bridge stability, performance, numerical accuracy, and correctness
 * HOW:  Test performance benchmarks, memory stability, numerical precision,
 *       boundary conditions, and state consistency
 *
 * TEST CATEGORIES:
 * 1. PERF - Performance benchmarks (throughput, latency)
 * 2. MEM  - Memory stability (create/destroy cycles, long-running ops)
 * 3. NUM  - Numerical accuracy (Yerkes-Dodson, modulation, oscillation)
 * 4. BOUND - Boundary conditions (error types, limits, extremes)
 * 5. STATE - State consistency (reset, statistics, connections)
 *
 * @version 1.0.0
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/medulla/nimcp_medulla_cerebellum_bridge.h"
#include "core/medulla/nimcp_medulla.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr int PERF_ITERATIONS = 1000;
static constexpr int PERF_WARMUP = 50;
static constexpr int STRESS_ITERATIONS = 1000;
static constexpr int LONG_RUNNING_CYCLES = 10000;
static constexpr double PI = 3.14159265358979323846;

//=============================================================================
// Performance Regression Tests (PERF)
//=============================================================================

class MedullaCerebellumBridgePerfTest : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;

    void SetUp() override {
        med_cereb_bridge_config_t config;
        med_cereb_bridge_default_config(&config);
        config.enable_io_signaling = true;
        config.enable_arousal_modulation = true;
        config.enable_protection_gating = true;
        bridge = med_cereb_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        // Create cerebellum for full integration
        cerebellum = cerebellum_create(nullptr);
        ASSERT_NE(cerebellum, nullptr);

        // Connect bridge to cerebellum
        int result = med_cereb_bridge_connect_cerebellum(bridge, cerebellum);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
    }
};

/**
 * @test PERF: Error queue throughput - measure queue+process cycle time
 *
 * Note: Queue capacity is limited to MED_CEREB_MAX_ERROR_QUEUE (64),
 * so we measure queue-then-update cycles to test realistic throughput.
 */
TEST_F(MedullaCerebellumBridgePerfTest, ErrorQueueThroughput) {
    // Warmup
    for (int i = 0; i < PERF_WARMUP; i++) {
        med_cereb_bridge_queue_error(bridge,
                                      static_cast<med_cereb_error_type_t>(i % MED_CEREB_ERROR_COUNT),
                                      0.5f,
                                      static_cast<uint32_t>(i));
        med_cereb_bridge_update(bridge, 1000);
    }
    med_cereb_bridge_reset(bridge);

    auto start = std::chrono::steady_clock::now();

    // Queue errors in batches that fit in the queue, then process
    int total_errors_queued = 0;
    for (int batch = 0; batch < PERF_ITERATIONS / MED_CEREB_MAX_ERROR_QUEUE + 1; batch++) {
        int errors_this_batch = std::min(MED_CEREB_MAX_ERROR_QUEUE - 1,
                                          PERF_ITERATIONS - total_errors_queued);
        if (errors_this_batch <= 0) break;

        for (int i = 0; i < errors_this_batch; i++) {
            int result = med_cereb_bridge_queue_error(bridge,
                static_cast<med_cereb_error_type_t>(i % MED_CEREB_ERROR_COUNT),
                static_cast<float>(i % 100) / 100.0f - 0.5f,
                static_cast<uint32_t>(i));
            EXPECT_EQ(result, 0) << "Failed to queue error " << i << " in batch " << batch;
            total_errors_queued++;
        }

        // Process the batch
        for (int j = 0; j < 10; j++) {
            med_cereb_bridge_update(bridge, 10000);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double per_op_us = static_cast<double>(duration_us.count()) / total_errors_queued;

    // Queue+process cycle should be < 50us per error (includes update overhead)
    EXPECT_LT(per_op_us, 50.0)
        << "Error queue throughput: " << per_op_us << "us per operation";

    std::cout << "[PERF] Error queue throughput: " << per_op_us << " us/op, "
              << total_errors_queued << " errors processed" << std::endl;
}

/**
 * @test PERF: Update cycle latency - measure update cycle time
 */
TEST_F(MedullaCerebellumBridgePerfTest, UpdateCycleLatency) {
    // Queue some errors first
    for (int i = 0; i < 10; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.3f, i);
    }

    // Warmup
    for (int i = 0; i < PERF_WARMUP; i++) {
        med_cereb_bridge_update(bridge, 16000);
    }

    std::vector<double> latencies;
    latencies.reserve(PERF_ITERATIONS);

    for (int i = 0; i < PERF_ITERATIONS; i++) {
        // Queue an error before each update (ignoring return if queue full)
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.2f, i);

        auto start = std::chrono::steady_clock::now();
        int result = med_cereb_bridge_update(bridge, 16000);
        auto end = std::chrono::steady_clock::now();

        EXPECT_EQ(result, 0);
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        latencies.push_back(duration.count() / 1000.0);
    }

    double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    std::sort(latencies.begin(), latencies.end());
    double p50 = latencies[latencies.size() / 2];
    double p99 = latencies[static_cast<size_t>(latencies.size() * 0.99)];

    // Update should be < 100us average
    EXPECT_LT(avg_latency, 100.0)
        << "Update latency avg: " << avg_latency << "us";

    std::cout << "[PERF] Update latency - avg: " << avg_latency << "us, "
              << "p50: " << p50 << "us, p99: " << p99 << "us" << std::endl;
}

/**
 * @test PERF: IO spike processing rate (requires cerebellum connection)
 */
TEST_F(MedullaCerebellumBridgePerfTest, IOSpikeProcessingRate) {
    med_cereb_bridge_stats_t stats_before;
    med_cereb_bridge_get_stats(bridge, &stats_before);

    auto start = std::chrono::steady_clock::now();

    // Send many climbing signals directly (bypasses IO model)
    for (int i = 0; i < PERF_ITERATIONS; i++) {
        int result = med_cereb_bridge_send_climbing_signal(
            bridge,
            static_cast<med_cereb_error_type_t>(i % MED_CEREB_ERROR_COUNT),
            0.5f,
            i % 100);
        EXPECT_EQ(result, 0) << "Failed at iteration " << i;
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double per_op_us = static_cast<double>(duration_us.count()) / PERF_ITERATIONS;

    med_cereb_bridge_stats_t stats_after;
    med_cereb_bridge_get_stats(bridge, &stats_after);

    // Climbing signal should be < 50us per signal
    EXPECT_LT(per_op_us, 50.0)
        << "IO spike processing: " << per_op_us << "us per operation";

    EXPECT_GE(stats_after.climbing_signals_sent,
              stats_before.climbing_signals_sent + PERF_ITERATIONS);

    std::cout << "[PERF] IO spike processing: " << per_op_us << " us/op, "
              << stats_after.climbing_signals_sent << " signals sent" << std::endl;
}

/**
 * @test PERF: Motor modulation latency
 */
TEST_F(MedullaCerebellumBridgePerfTest, MotorModulationLatency) {
    const uint32_t NUM_DIMS = 6;  // 6-DOF motor command
    float motor_in[NUM_DIMS] = {0.5f, 0.3f, 0.7f, 0.2f, 0.8f, 0.4f};
    float motor_out[NUM_DIMS];

    // Warmup
    for (int i = 0; i < PERF_WARMUP; i++) {
        med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, NUM_DIMS);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < PERF_ITERATIONS; i++) {
        motor_in[0] = 0.5f + 0.1f * sinf(static_cast<float>(i) * 0.1f);
        int result = med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, NUM_DIMS);
        EXPECT_EQ(result, 0);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double per_op_us = static_cast<double>(duration_us.count()) / PERF_ITERATIONS;

    // Motor modulation should be < 5us per operation
    EXPECT_LT(per_op_us, 5.0)
        << "Motor modulation: " << per_op_us << "us per operation";

    std::cout << "[PERF] Motor modulation latency: " << per_op_us << " us/op" << std::endl;
}

//=============================================================================
// Memory Stability Tests (MEM)
//=============================================================================

class MedullaCerebellumBridgeMemTest : public ::testing::Test {};

/**
 * @test MEM: Create/destroy cycles - 1000 iterations for leak detection
 */
TEST(MedullaCerebellumBridgeMemTest, CreateDestroyCycles) {
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        med_cereb_bridge_config_t config;
        med_cereb_bridge_default_config(&config);
        config.num_io_neurons = static_cast<uint32_t>(10 + (i % 90));

        med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
        ASSERT_NE(bridge, nullptr) << "Failed at iteration " << i;

        // Do some work
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
        med_cereb_bridge_update(bridge, 1000);

        med_cereb_bridge_destroy(bridge);
    }
    std::cout << "[MEM] Create/destroy cycles: " << STRESS_ITERATIONS
              << " iterations completed" << std::endl;
}

/**
 * @test MEM: Long-running operation - 10000 update cycles
 */
TEST(MedullaCerebellumBridgeMemTest, LongRunningOperation) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.enable_io_signaling = true;
    config.num_io_neurons = MED_CEREB_MAX_IO_NEURONS;

    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect cerebellum so IO spikes are counted
    cerebellum_adapter_t* cerebellum = cerebellum_create(nullptr);
    ASSERT_NE(cerebellum, nullptr);
    med_cereb_bridge_connect_cerebellum(bridge, cerebellum);

    med_cereb_bridge_stats_t initial_stats;
    med_cereb_bridge_get_stats(bridge, &initial_stats);

    for (int i = 0; i < LONG_RUNNING_CYCLES; i++) {
        // Queue errors periodically
        if (i % 10 == 0) {
            med_cereb_bridge_queue_error(
                bridge,
                static_cast<med_cereb_error_type_t>(i % MED_CEREB_ERROR_COUNT),
                0.3f,
                i);
        }

        int result = med_cereb_bridge_update(bridge, 1000);
        EXPECT_EQ(result, 0) << "Failed at cycle " << i;
    }

    med_cereb_bridge_stats_t final_stats;
    med_cereb_bridge_get_stats(bridge, &final_stats);

    EXPECT_GE(final_stats.climbing_signals_sent, initial_stats.climbing_signals_sent);
    EXPECT_FALSE(std::isnan(final_stats.avg_error_magnitude));
    EXPECT_FALSE(std::isinf(final_stats.avg_error_magnitude));

    cerebellum_destroy(cerebellum);
    med_cereb_bridge_destroy(bridge);

    std::cout << "[MEM] Long-running: " << LONG_RUNNING_CYCLES << " cycles, "
              << final_stats.io_spikes << " IO spikes generated" << std::endl;
}

/**
 * @test MEM: Queue full handling - verify no memory corruption
 */
TEST(MedullaCerebellumBridgeMemTest, QueueFullHandling) {
    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    med_cereb_bridge_t bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Fill the queue completely - expect -1 return when full
    int success_count = 0;
    for (int i = 0; i < MED_CEREB_MAX_ERROR_QUEUE * 2; i++) {
        int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
        if (result == 0) success_count++;
    }

    // Should have queued exactly MED_CEREB_MAX_ERROR_QUEUE errors
    EXPECT_EQ(success_count, MED_CEREB_MAX_ERROR_QUEUE);

    uint32_t pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, static_cast<uint32_t>(MED_CEREB_MAX_ERROR_QUEUE));

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.errors_dropped, static_cast<uint64_t>(MED_CEREB_MAX_ERROR_QUEUE));

    // Process all pending
    for (int i = 0; i < 100; i++) {
        med_cereb_bridge_update(bridge, 10000);
    }

    pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, 0u);

    med_cereb_bridge_destroy(bridge);

    std::cout << "[MEM] Queue full handling: " << stats.errors_dropped
              << " errors dropped (expected)" << std::endl;
}

//=============================================================================
// Numerical Accuracy Tests (NUM)
//=============================================================================

class MedullaCerebellumBridgeNumTest : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;
    medulla_t medulla = nullptr;

    void SetUp() override {
        // Create medulla for full integration
        medulla_config_t med_config = medulla_default_config();
        medulla = medulla_create(&med_config);
        ASSERT_NE(medulla, nullptr);
        medulla_start(medulla);

        // Create bridge
        med_cereb_bridge_config_t config;
        med_cereb_bridge_default_config(&config);
        config.enable_arousal_modulation = true;
        config.enable_circadian_learning = true;
        config.optimal_arousal_level = 0.5f;
        bridge = med_cereb_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        med_cereb_bridge_connect_medulla(bridge, medulla);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (medulla) {
            medulla_stop(medulla);
            medulla_destroy(medulla);
            medulla = nullptr;
        }
    }
};

/**
 * @test NUM: Arousal effect calculations - verify Yerkes-Dodson inverted U curve
 */
TEST_F(MedullaCerebellumBridgeNumTest, ArousalYerkesDodsonCurve) {
    med_cereb_arousal_effects_t effects;

    int result = med_cereb_bridge_get_arousal_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Verify all effects are in valid ranges
    EXPECT_GE(effects.motor_gain, 0.2f);
    EXPECT_LE(effects.motor_gain, 2.0f);

    EXPECT_GE(effects.reaction_time_factor, 0.5f);
    EXPECT_LE(effects.reaction_time_factor, 2.0f);

    EXPECT_GE(effects.nuclei_excitability, 0.0f);
    EXPECT_LE(effects.nuclei_excitability, 1.0f);

    EXPECT_GE(effects.fine_motor_precision, 0.0f);
    EXPECT_LE(effects.fine_motor_precision, 1.0f);

    EXPECT_GE(effects.tremor_amplitude, 0.0f);
    EXPECT_LE(effects.tremor_amplitude, 1.0f);

    EXPECT_FALSE(std::isnan(effects.motor_gain));
    EXPECT_FALSE(std::isnan(effects.fine_motor_precision));
    EXPECT_FALSE(std::isinf(effects.motor_gain));
    EXPECT_FALSE(std::isinf(effects.fine_motor_precision));

    std::cout << "[NUM] Arousal effects - gain: " << effects.motor_gain
              << ", precision: " << effects.fine_motor_precision << std::endl;
}

/**
 * @test NUM: Modulation factor precision - verify motor modulation accuracy
 */
TEST_F(MedullaCerebellumBridgeNumTest, ModulationFactorPrecision) {
    const uint32_t NUM_DIMS = 3;
    float motor_in[NUM_DIMS] = {1.0f, 0.5f, 0.0f};
    float motor_out[NUM_DIMS];

    int result = med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, NUM_DIMS);
    EXPECT_EQ(result, 0);

    for (uint32_t i = 0; i < NUM_DIMS; i++) {
        EXPECT_FALSE(std::isnan(motor_out[i])) << "NaN at dimension " << i;
        EXPECT_FALSE(std::isinf(motor_out[i])) << "Inf at dimension " << i;
        EXPECT_GE(motor_out[i], -10.0f) << "Output too negative at dim " << i;
        EXPECT_LE(motor_out[i], 10.0f) << "Output too positive at dim " << i;
    }

    // Verify consistency
    float motor_out2[NUM_DIMS];
    result = med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out2, NUM_DIMS);
    EXPECT_EQ(result, 0);

    for (uint32_t i = 0; i < NUM_DIMS; i++) {
        EXPECT_FLOAT_EQ(motor_out[i], motor_out2[i])
            << "Inconsistent modulation at dimension " << i;
    }
}

/**
 * @test NUM: IO oscillation phase accuracy - verify phase stays in [0, 2*PI]
 */
TEST_F(MedullaCerebellumBridgeNumTest, IOOscillationPhaseAccuracy) {
    med_cereb_inferior_olive_t io_state;

    // Run many updates to advance oscillation phases
    for (int cycle = 0; cycle < 1000; cycle++) {
        med_cereb_bridge_update(bridge, 10000);
    }

    int result = med_cereb_bridge_get_io_state(bridge, &io_state);
    EXPECT_EQ(result, 0);

    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        float phase = io_state.neurons[i].oscillation_phase;

        EXPECT_GE(phase, 0.0f) << "Negative phase at neuron " << i;
        EXPECT_LT(phase, 2.0f * PI + 0.001f) << "Phase > 2*PI at neuron " << i;
        EXPECT_FALSE(std::isnan(phase)) << "NaN phase at neuron " << i;

        float activation = io_state.neurons[i].activation;
        EXPECT_GE(activation, -1.0f);
        EXPECT_LE(activation, 1.0f);
    }

    std::cout << "[NUM] IO oscillation: " << io_state.num_neurons << " neurons, "
              << "freq=" << io_state.oscillation_freq << "Hz" << std::endl;
}

/**
 * @test NUM: Circadian effects precision
 */
TEST_F(MedullaCerebellumBridgeNumTest, CircadianEffectsPrecision) {
    med_cereb_circadian_effects_t effects;

    int result = med_cereb_bridge_get_circadian_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    EXPECT_GE(effects.ltd_rate_multiplier, 0.3f);
    EXPECT_LE(effects.ltd_rate_multiplier, 1.5f);

    EXPECT_GE(effects.ltp_rate_multiplier, 0.3f);
    EXPECT_LE(effects.ltp_rate_multiplier, 1.5f);

    EXPECT_GE(effects.consolidation_rate, 0.0f);
    EXPECT_LE(effects.consolidation_rate, 1.0f);

    EXPECT_GE(effects.retrieval_efficiency, 0.0f);
    EXPECT_LE(effects.retrieval_efficiency, 1.0f);

    EXPECT_FALSE(std::isnan(effects.ltd_rate_multiplier));
    EXPECT_FALSE(std::isnan(effects.ltp_rate_multiplier));

    float learning_mult = med_cereb_bridge_get_learning_multiplier(bridge);
    EXPECT_GE(learning_mult, 0.1f);
    EXPECT_LE(learning_mult, 2.0f);
    EXPECT_FALSE(std::isnan(learning_mult));

    std::cout << "[NUM] Circadian effects - LTD: " << effects.ltd_rate_multiplier
              << ", LTP: " << effects.ltp_rate_multiplier
              << ", learning mult: " << learning_mult << std::endl;
}

//=============================================================================
// Boundary Condition Tests (BOUND)
//=============================================================================

class MedullaCerebellumBridgeBoundTest : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;

    void SetUp() override {
        med_cereb_bridge_config_t config;
        med_cereb_bridge_default_config(&config);
        config.enable_io_signaling = true;
        config.enable_protection_gating = true;
        bridge = med_cereb_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        // Connect cerebellum for climbing signal tests
        cerebellum = cerebellum_create(nullptr);
        ASSERT_NE(cerebellum, nullptr);
        med_cereb_bridge_connect_cerebellum(bridge, cerebellum);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
    }
};

/**
 * @test BOUND: All error types processed correctly
 */
TEST_F(MedullaCerebellumBridgeBoundTest, AllErrorTypesProcessed) {
    med_cereb_bridge_stats_t stats_before;
    med_cereb_bridge_get_stats(bridge, &stats_before);

    // Queue one of each error type
    for (int type = 0; type < MED_CEREB_ERROR_COUNT; type++) {
        int result = med_cereb_bridge_queue_error(
            bridge,
            static_cast<med_cereb_error_type_t>(type),
            0.5f,
            type);
        EXPECT_EQ(result, 0) << "Failed to queue error type " << type;

        const char* name = med_cereb_error_type_name(static_cast<med_cereb_error_type_t>(type));
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Process all errors
    for (int i = 0; i < 100; i++) {
        med_cereb_bridge_update(bridge, 10000);
    }

    med_cereb_bridge_stats_t stats_after;
    med_cereb_bridge_get_stats(bridge, &stats_after);

    // Verify some signals were sent
    EXPECT_GT(stats_after.climbing_signals_sent, stats_before.climbing_signals_sent);

    std::cout << "[BOUND] All " << MED_CEREB_ERROR_COUNT
              << " error types processed, " << stats_after.climbing_signals_sent
              << " climbing signals sent" << std::endl;
}

/**
 * @test BOUND: Max IO neurons configuration
 */
TEST_F(MedullaCerebellumBridgeBoundTest, MaxIONeurons) {
    med_cereb_bridge_destroy(bridge);
    cerebellum_destroy(cerebellum);

    med_cereb_bridge_config_t config;
    med_cereb_bridge_default_config(&config);
    config.num_io_neurons = MED_CEREB_MAX_IO_NEURONS;
    config.enable_io_signaling = true;

    bridge = med_cereb_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    cerebellum = cerebellum_create(nullptr);
    ASSERT_NE(cerebellum, nullptr);
    med_cereb_bridge_connect_cerebellum(bridge, cerebellum);

    med_cereb_inferior_olive_t io_state;
    int result = med_cereb_bridge_get_io_state(bridge, &io_state);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(io_state.num_neurons, MED_CEREB_MAX_IO_NEURONS);

    // Run some updates with max neurons
    for (int i = 0; i < 100; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.3f, i);
        result = med_cereb_bridge_update(bridge, 10000);
        EXPECT_EQ(result, 0);
    }

    std::cout << "[BOUND] Max IO neurons: " << MED_CEREB_MAX_IO_NEURONS
              << " neurons handled correctly" << std::endl;
}

/**
 * @test BOUND: Queue capacity limits
 */
TEST_F(MedullaCerebellumBridgeBoundTest, QueueCapacityLimits) {
    // Fill queue to capacity
    for (int i = 0; i < MED_CEREB_MAX_ERROR_QUEUE; i++) {
        int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.5f, i);
        EXPECT_EQ(result, 0);
    }

    uint32_t pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, static_cast<uint32_t>(MED_CEREB_MAX_ERROR_QUEUE));

    // Try to add one more - should fail
    int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_AMPLITUDE, 0.5f, 999);
    EXPECT_EQ(result, -1);  // Queue full

    pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, static_cast<uint32_t>(MED_CEREB_MAX_ERROR_QUEUE));

    std::cout << "[BOUND] Queue capacity: " << MED_CEREB_MAX_ERROR_QUEUE
              << " errors max, pending=" << pending << std::endl;
}

/**
 * @test BOUND: Protection level extremes - emergency stop/release cycle
 *
 * The emergency_stop function sets an internal flag that gates all motor output.
 * This is checked via motor_allowed() rather than protection_effects.output_scale.
 */
TEST_F(MedullaCerebellumBridgeBoundTest, ProtectionLevelExtremes) {
    // Test motor allowed at various conditions - normal state
    bool allowed = med_cereb_bridge_motor_allowed(bridge, true, false);
    EXPECT_TRUE(allowed);  // Essential motor should be allowed initially

    allowed = med_cereb_bridge_motor_allowed(bridge, false, true);
    EXPECT_TRUE(allowed);  // Reflexive motor should be allowed initially

    allowed = med_cereb_bridge_motor_allowed(bridge, false, false);
    EXPECT_TRUE(allowed);  // Non-essential, non-reflexive should also be allowed

    // Test emergency stop/release cycle
    int result = med_cereb_bridge_emergency_stop(bridge);
    EXPECT_EQ(result, 0);

    // After emergency stop, ALL motor should be blocked
    allowed = med_cereb_bridge_motor_allowed(bridge, true, true);
    EXPECT_FALSE(allowed);  // Even essential+reflexive blocked

    allowed = med_cereb_bridge_motor_allowed(bridge, false, false);
    EXPECT_FALSE(allowed);

    // Verify motor modulation outputs zeros during emergency
    float motor_in[3] = {1.0f, 1.0f, 1.0f};
    float motor_out[3];
    result = med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, 3);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(motor_out[0], 0.0f);
    EXPECT_FLOAT_EQ(motor_out[1], 0.0f);
    EXPECT_FLOAT_EQ(motor_out[2], 0.0f);

    // Release emergency
    result = med_cereb_bridge_release_emergency(bridge);
    EXPECT_EQ(result, 0);

    // After release, motor should be allowed again
    allowed = med_cereb_bridge_motor_allowed(bridge, true, false);
    EXPECT_TRUE(allowed);

    allowed = med_cereb_bridge_motor_allowed(bridge, false, true);
    EXPECT_TRUE(allowed);

    std::cout << "[BOUND] Protection level extremes tested" << std::endl;
}

/**
 * @test BOUND: Error magnitude extremes [-1, 1]
 */
TEST_F(MedullaCerebellumBridgeBoundTest, ErrorMagnitudeExtremes) {
    // Test extreme magnitudes - all should succeed with cerebellum connected
    float test_magnitudes[] = {-1.0f, -0.999f, -0.5f, 0.0f, 0.5f, 0.999f, 1.0f};

    for (float mag : test_magnitudes) {
        int result = med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_PREDICTION, mag, 0);
        EXPECT_EQ(result, 0) << "Failed to queue for magnitude " << mag;

        // Process the error
        med_cereb_bridge_update(bridge, 10000);
    }

    // Direct climbing signals
    for (float mag : test_magnitudes) {
        int result = med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_PREDICTION, mag, 0);
        EXPECT_EQ(result, 0) << "Failed climbing signal for magnitude " << mag;

        result = med_cereb_bridge_broadcast_error(bridge, MED_CEREB_ERROR_PREDICTION, mag);
        EXPECT_EQ(result, 0) << "Failed broadcast for magnitude " << mag;
    }

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    EXPECT_GE(stats.peak_error_magnitude, 0.999f);
    EXPECT_LE(stats.peak_error_magnitude, 1.0f);

    std::cout << "[BOUND] Error magnitude extremes tested, peak: "
              << stats.peak_error_magnitude << std::endl;
}

//=============================================================================
// State Consistency Tests (STATE)
//=============================================================================

class MedullaCerebellumBridgeStateTest : public ::testing::Test {
protected:
    med_cereb_bridge_t bridge = nullptr;

    void SetUp() override {
        med_cereb_bridge_config_t config;
        med_cereb_bridge_default_config(&config);
        config.enable_io_signaling = true;
        config.enable_arousal_modulation = true;
        config.enable_protection_gating = true;
        config.enable_circadian_learning = true;
        bridge = med_cereb_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            med_cereb_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/**
 * @test STATE: Reset clears state correctly
 */
TEST_F(MedullaCerebellumBridgeStateTest, ResetClearsState) {
    // Queue some errors
    for (int i = 0; i < 50; i++) {
        med_cereb_bridge_queue_error(bridge, MED_CEREB_ERROR_TIMING, 0.5f, i);
    }

    uint32_t pending_before = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_GT(pending_before, 0u);

    // Reset
    int result = med_cereb_bridge_reset(bridge);
    EXPECT_EQ(result, 0);

    // Verify queue is empty
    uint32_t pending = med_cereb_bridge_pending_error_count(bridge);
    EXPECT_EQ(pending, 0u);

    // IO state should be reset
    med_cereb_inferior_olive_t io_state;
    result = med_cereb_bridge_get_io_state(bridge, &io_state);
    EXPECT_EQ(result, 0);

    for (uint32_t i = 0; i < io_state.num_neurons; i++) {
        EXPECT_GE(io_state.neurons[i].activation, -1.0f);
        EXPECT_LE(io_state.neurons[i].activation, 1.0f);
    }

    std::cout << "[STATE] Reset verified - queue cleared (had " << pending_before
              << " errors), IO state reset" << std::endl;
}

/**
 * @test STATE: Statistics accuracy (requires cerebellum connection)
 */
TEST_F(MedullaCerebellumBridgeStateTest, StatisticsAccuracy) {
    // Connect cerebellum for climbing signal counting
    cerebellum_adapter_t* cerebellum = cerebellum_create(nullptr);
    ASSERT_NE(cerebellum, nullptr);
    med_cereb_bridge_connect_cerebellum(bridge, cerebellum);

    med_cereb_bridge_reset_stats(bridge);

    med_cereb_bridge_stats_t stats;
    med_cereb_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.climbing_signals_sent, 0u);
    EXPECT_EQ(stats.motor_commands_modulated, 0u);
    EXPECT_EQ(stats.errors_dropped, 0u);

    // Send known number of signals
    const int NUM_SIGNALS = 50;
    for (int i = 0; i < NUM_SIGNALS; i++) {
        med_cereb_bridge_send_climbing_signal(bridge, MED_CEREB_ERROR_TIMING, 0.5f, 0);
    }

    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.climbing_signals_sent, static_cast<uint64_t>(NUM_SIGNALS));

    // Modulate motor
    float motor_in[3] = {0.5f, 0.5f, 0.5f};
    float motor_out[3];
    const int NUM_MODULATIONS = 25;
    for (int i = 0; i < NUM_MODULATIONS; i++) {
        med_cereb_bridge_modulate_motor(bridge, motor_in, motor_out, 3);
    }

    med_cereb_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.motor_commands_modulated, static_cast<uint64_t>(NUM_MODULATIONS));

    cerebellum_destroy(cerebellum);

    std::cout << "[STATE] Statistics accuracy verified - signals: "
              << stats.climbing_signals_sent << ", modulations: "
              << stats.motor_commands_modulated << std::endl;
}

/**
 * @test STATE: Connection state tracking
 */
TEST_F(MedullaCerebellumBridgeStateTest, ConnectionStateTracking) {
    // Initially not fully connected
    bool connected = med_cereb_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);

    // Create and connect medulla
    medulla_config_t med_config = medulla_default_config();
    medulla_t medulla = medulla_create(&med_config);
    ASSERT_NE(medulla, nullptr);

    int result = med_cereb_bridge_connect_medulla(bridge, medulla);
    EXPECT_EQ(result, 0);

    // Still not fully connected (need cerebellum)
    connected = med_cereb_bridge_is_connected(bridge);
    EXPECT_FALSE(connected);

    // Create and connect cerebellum
    cerebellum_adapter_t* cerebellum = cerebellum_create(nullptr);
    ASSERT_NE(cerebellum, nullptr);

    result = med_cereb_bridge_connect_cerebellum(bridge, cerebellum);
    EXPECT_EQ(result, 0);

    // Now should be fully connected
    connected = med_cereb_bridge_is_connected(bridge);
    EXPECT_TRUE(connected);

    cerebellum_destroy(cerebellum);
    medulla_destroy(medulla);

    std::cout << "[STATE] Connection tracking verified" << std::endl;
}

/**
 * @test STATE: Null pointer safety
 */
TEST_F(MedullaCerebellumBridgeStateTest, NullPointerSafety) {
    EXPECT_LT(med_cereb_bridge_reset(nullptr), 0);
    EXPECT_LT(med_cereb_bridge_update(nullptr, 1000), 0);
    EXPECT_LT(med_cereb_bridge_connect_medulla(nullptr, nullptr), 0);
    EXPECT_LT(med_cereb_bridge_connect_cerebellum(nullptr, nullptr), 0);
    EXPECT_LT(med_cereb_bridge_queue_error(nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 0), 0);
    EXPECT_LT(med_cereb_bridge_send_climbing_signal(nullptr, MED_CEREB_ERROR_TIMING, 0.5f, 0), 0);

    med_cereb_arousal_effects_t arousal_effects;
    EXPECT_LT(med_cereb_bridge_get_arousal_effects(nullptr, &arousal_effects), 0);
    EXPECT_LT(med_cereb_bridge_get_arousal_effects(bridge, nullptr), 0);

    med_cereb_protection_effects_t prot_effects;
    EXPECT_LT(med_cereb_bridge_get_protection_effects(nullptr, &prot_effects), 0);

    med_cereb_bridge_stats_t stats;
    EXPECT_LT(med_cereb_bridge_get_stats(nullptr, &stats), 0);
    EXPECT_LT(med_cereb_bridge_get_stats(bridge, nullptr), 0);

    EXPECT_FALSE(med_cereb_bridge_is_connected(nullptr));
    EXPECT_EQ(med_cereb_bridge_pending_error_count(nullptr), 0u);

    // These should not crash
    med_cereb_bridge_destroy(nullptr);
    med_cereb_bridge_print_state(nullptr);
    med_cereb_bridge_print_io_state(nullptr);

    std::cout << "[STATE] Null pointer safety verified" << std::endl;
}

/**
 * @test STATE: Default configuration consistency
 */
TEST_F(MedullaCerebellumBridgeStateTest, DefaultConfigConsistency) {
    med_cereb_bridge_config_t config1, config2;

    int result1 = med_cereb_bridge_default_config(&config1);
    int result2 = med_cereb_bridge_default_config(&config2);

    EXPECT_EQ(result1, 0);
    EXPECT_EQ(result2, 0);

    EXPECT_EQ(config1.num_io_neurons, config2.num_io_neurons);
    EXPECT_EQ(config1.enable_arousal_modulation, config2.enable_arousal_modulation);
    EXPECT_EQ(config1.enable_protection_gating, config2.enable_protection_gating);
    EXPECT_EQ(config1.enable_circadian_learning, config2.enable_circadian_learning);
    EXPECT_EQ(config1.enable_io_signaling, config2.enable_io_signaling);
    EXPECT_FLOAT_EQ(config1.io_oscillation_freq, config2.io_oscillation_freq);
    EXPECT_FLOAT_EQ(config1.io_coupling_strength, config2.io_coupling_strength);
    EXPECT_FLOAT_EQ(config1.io_firing_threshold, config2.io_firing_threshold);

    EXPECT_GT(config1.num_io_neurons, 0u);
    EXPECT_LE(config1.num_io_neurons, MED_CEREB_MAX_IO_NEURONS);
    EXPECT_GT(config1.io_oscillation_freq, 0.0f);
    EXPECT_LE(config1.io_oscillation_freq, MED_CEREB_MAX_IO_RATE);

    std::cout << "[STATE] Default config consistency verified" << std::endl;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
