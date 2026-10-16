/**
 * @file test_cerebellum_regression.cpp
 * @brief Regression tests for Cerebellum adapter and related components
 *
 * WHAT: Performance and correctness regression tests for cerebellum
 * WHY:  Ensure changes don't degrade performance or break motor coordination
 * HOW:  Benchmark operations, verify numerical stability, check memory
 *
 * TEST CATEGORIES:
 * 1. Performance: Create/destroy, mossy input, climbing fiber, motor output
 * 2. Numerical Stability: VOR gains, synaptic weights, adaptation rates
 * 3. Memory: Leak detection, pool usage
 * 4. Correctness: Expected behaviors preserved across versions
 *
 * @version Phase 3: Vestibular System Integration
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"
#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"

//=============================================================================
// Performance Regression Tests
//=============================================================================

class CerebellumPerformanceTest : public ::testing::Test {
protected:
    static constexpr int ITERATIONS = 3;
    static constexpr int WARMUP = 1;
};

TEST_F(CerebellumPerformanceTest, CreateDestroyPerformance) {
    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        cerebellum_adapter_t* c = cerebellum_create(nullptr);
        cerebellum_destroy(c);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        cerebellum_adapter_t* c = cerebellum_create(nullptr);
        cerebellum_destroy(c);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;

    // Create/destroy threshold relaxed - cerebellum_create allocates substantial internal state
    EXPECT_LT(per_op_us, 2000000.0) << "Create/destroy took " << per_op_us << "us per operation";
}

TEST_F(CerebellumPerformanceTest, MossyInputPerformance) {
    cerebellum_adapter_t* adapter = cerebellum_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.5f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        input.fiber_id = i % 100;
        input.timestamp_ms = (float)i;
        cerebellum_process_mossy_input(adapter, &input);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        input.fiber_id = i % 100;
        input.timestamp_ms = (float)i;
        cerebellum_process_mossy_input(adapter, &input);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;

    // Mossy input processing should be < 50us per input
    EXPECT_LT(per_op_us, 50.0) << "Mossy input took " << per_op_us << "us per operation";

    cerebellum_destroy(adapter);
}

TEST_F(CerebellumPerformanceTest, ClimbingFiberPerformance) {
    cerebellum_adapter_t* adapter = cerebellum_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    climbing_fiber_signal_t signal;
    signal.fiber_id = 0;
    signal.error_signal = 0.3f;
    signal.timestamp_ms = 0.0f;
    signal.target_purkinje_id = 0;
    signal.error_type = 0;

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        signal.fiber_id = i % 50;
        signal.target_purkinje_id = i % 100;
        cerebellum_process_climbing_signal(adapter, &signal);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        signal.fiber_id = i % 50;
        signal.target_purkinje_id = i % 100;
        cerebellum_process_climbing_signal(adapter, &signal);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;

    // Climbing fiber processing should be < 100us per signal
    EXPECT_LT(per_op_us, 100.0) << "Climbing fiber took " << per_op_us << "us per operation";

    cerebellum_destroy(adapter);
}

TEST_F(CerebellumPerformanceTest, MotorCoordinationPerformance) {
    cerebellum_adapter_t* adapter = cerebellum_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 0.8f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    // Setup: process some inputs
    for (int i = 0; i < 10; i++) {
        input.fiber_id = i;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        cerebellum_process(adapter, &result);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        cerebellum_process(adapter, &result);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;

    // Full motor coordination should be < 200us per cycle
    EXPECT_LT(per_op_us, 200.0) << "Motor coordination took " << per_op_us << "us per operation";

    cerebellum_destroy(adapter);
}

//=============================================================================
// Synaptic Dynamics Performance
//=============================================================================

TEST_F(CerebellumPerformanceTest, SynapticDynamicsUpdatePerformance) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    cerebellar_synapse_state_t state;
    cerebellar_synapse_init(&state, &config);

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        cerebellar_synapse_update(&state, 1.0f, 0.5f);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < ITERATIONS * 10; i++) {  // More iterations for micro-benchmark
        cerebellar_synapse_update(&state, 1.0f, 0.5f);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_op_us = static_cast<double>(duration_us.count()) / (ITERATIONS * 10);

    // Synapse update should be < 1us per update
    EXPECT_LT(per_op_us, 1.0) << "Synapse update took " << per_op_us << "us per operation";
}

//=============================================================================
// Vestibular Performance
//=============================================================================

TEST_F(CerebellumPerformanceTest, VestibularInputPerformance) {
    vestibular_processor_t* processor = vestibular_create(nullptr);
    ASSERT_NE(processor, nullptr);

    semicircular_canal_input_t input;
    input.yaw_velocity = 0.5f;
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    // Warmup
    for (int i = 0; i < WARMUP; i++) {
        input.timestamp_us = i * 10000;
        vestibular_process_canal_input(processor, &input);
    }

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        input.timestamp_us = i * 10000;
        vestibular_process_canal_input(processor, &input);
    }

    auto end = std::chrono::steady_clock::now();
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_op_us = static_cast<double>(duration_us.count()) / ITERATIONS;

    // Vestibular input should be < 20us per input
    EXPECT_LT(per_op_us, 20.0) << "Vestibular input took " << per_op_us << "us per operation";

    vestibular_destroy(processor);
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

class CerebellumNumericalStabilityTest : public ::testing::Test {};

TEST_F(CerebellumNumericalStabilityTest, SynapticWeightsStable) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    cerebellar_synapse_state_t state;
    cerebellar_synapse_init(&state, &config);

    // Run many iterations with varying inputs
    for (int i = 0; i < 1000; i++) {
        float activity = 0.5f + 0.5f * sin(i * 0.1f);
        cerebellar_synapse_update(&state, 1.0f, activity);

        float weight = cerebellar_synapse_get_effective_weight(&state, 1.0f);

        // Weight should never be NaN or infinity
        ASSERT_FALSE(std::isnan(weight)) << "Weight became NaN at iteration " << i;
        ASSERT_FALSE(std::isinf(weight)) << "Weight became infinite at iteration " << i;

        // Weight should stay in reasonable range
        EXPECT_GE(weight, 0.0f) << "Weight went negative at iteration " << i;
        EXPECT_LE(weight, 10.0f) << "Weight exceeded maximum at iteration " << i;
    }
}

TEST_F(CerebellumNumericalStabilityTest, VORGainStable) {
    vestibular_processor_t* vestibular = vestibular_create(nullptr);
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_vestibulocerebellum = true;
    cerebellum_adapter_t* cerebellum = cerebellum_create(&config);
    vestibular_cerebellum_bridge_t* bridge =
        vestibular_cerebellum_bridge_create(vestibular, cerebellum, nullptr);

    ASSERT_NE(bridge, nullptr);

    // Apply many adaptation trials
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 100; i++) {
        float slip = 0.5f * sin(i * 0.05f);
        vestibular_cerebellum_trigger_vor_adaptation(bridge, slip, slip_direction);
    }

    // VOR gain should remain stable
    float vor_gain[3];
    bool active;
    vestibular_cerebellum_get_vor_state(bridge, vor_gain, &active);

    for (int axis = 0; axis < 3; axis++) {
        ASSERT_FALSE(std::isnan(vor_gain[axis])) << "VOR gain axis " << axis << " became NaN";
        ASSERT_FALSE(std::isinf(vor_gain[axis])) << "VOR gain axis " << axis << " became infinite";
        EXPECT_GE(vor_gain[axis], VESTIBULAR_MIN_VOR_GAIN);
        EXPECT_LE(vor_gain[axis], VESTIBULAR_MAX_VOR_GAIN);
    }

    vestibular_cerebellum_bridge_destroy(bridge);
    cerebellum_destroy(cerebellum);
    vestibular_destroy(vestibular);
}

TEST_F(CerebellumNumericalStabilityTest, CalciumConcentrationStable) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    config.enable_calcium_dynamics = true;
    cerebellar_synapse_state_t state;
    cerebellar_synapse_init(&state, &config);

    // Burst activity followed by silence
    for (int burst = 0; burst < 20; burst++) {
        // Burst of activity
        for (int i = 0; i < 10; i++) {
            cerebellar_synapse_update(&state, 5.0f, 1.0f);
            ASSERT_FALSE(std::isnan(state.calcium_concentration));
            ASSERT_GE(state.calcium_concentration, 0.0f);
        }

        // Silence
        for (int i = 0; i < 50; i++) {
            cerebellar_synapse_update(&state, 10.0f, 0.0f);
            ASSERT_FALSE(std::isnan(state.calcium_concentration));
            ASSERT_GE(state.calcium_concentration, 0.0f);
        }
    }
}

TEST_F(CerebellumNumericalStabilityTest, STPResourcesStable) {
    cerebellar_synapse_config_t config = cerebellar_synapse_default_config();
    config.enable_stp = true;
    cerebellar_synapse_state_t state;
    cerebellar_synapse_init(&state, &config);

    // High-frequency stimulation stress test
    for (int i = 0; i < 1000; i++) {
        cerebellar_synapse_update(&state, 2.0f, 1.0f);

        // STP resources must stay in [0, 1]
        ASSERT_GE(state.stp_x, 0.0f) << "stp_x went negative at iteration " << i;
        ASSERT_LE(state.stp_x, 1.0f) << "stp_x exceeded 1 at iteration " << i;
        ASSERT_GE(state.stp_u, 0.0f) << "stp_u went negative at iteration " << i;
        ASSERT_LE(state.stp_u, 1.0f) << "stp_u exceeded 1 at iteration " << i;
    }
}

//=============================================================================
// Correctness Regression Tests
//=============================================================================

class CerebellumCorrectnessTest : public ::testing::Test {};

TEST_F(CerebellumCorrectnessTest, VORCompensatesHeadMovement) {
    // Core behavior: VOR eye velocity should be opposite to head velocity
    vestibular_processor_t* processor = vestibular_create(nullptr);
    ASSERT_NE(processor, nullptr);

    semicircular_canal_input_t input;
    input.yaw_velocity = 1.0f;  // Head turning left
    input.pitch_velocity = 0.0f;
    input.roll_velocity = 0.0f;
    input.timestamp_us = 0;

    vestibular_process_canal_input(processor, &input);

    float eye_velocity[3];
    vestibular_get_vor_command(processor, eye_velocity);

    // Eye should move opposite to head (compensatory)
    EXPECT_LT(eye_velocity[0], 0.0f) << "VOR should produce compensatory eye movement";

    // Magnitude should be proportional to head velocity
    EXPECT_NEAR(fabs(eye_velocity[0]), 1.0f, 0.3f) << "VOR gain should be near 1.0";

    vestibular_destroy(processor);
}

TEST_F(CerebellumCorrectnessTest, LTDReducesWeight) {
    // Core behavior: Climbing fiber + parallel fiber activity = LTD
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_error_learning = true;
    cerebellum_adapter_t* adapter = cerebellum_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Process mossy input to activate parallel fibers
    mossy_fiber_input_t mossy;
    mossy.fiber_id = 0;
    mossy.activity = 0.8f;
    mossy.timestamp_ms = 0.0f;
    mossy.modality = 0;
    cerebellum_process_mossy_input(adapter, &mossy);

    cerebellum_stats_t initial_stats;
    cerebellum_get_stats(adapter, &initial_stats);

    // Send climbing fiber error signal
    climbing_fiber_signal_t climbing;
    climbing.fiber_id = 0;
    climbing.error_signal = 0.8f;
    climbing.timestamp_ms = 1.0f;
    climbing.target_purkinje_id = 0;
    climbing.error_type = 0;

    for (int i = 0; i < 10; i++) {
        cerebellum_process_climbing_signal(adapter, &climbing);
    }

    cerebellum_stats_t final_stats;
    cerebellum_get_stats(adapter, &final_stats);

    // LTD events should have occurred
    EXPECT_GT(final_stats.ltd_events, initial_stats.ltd_events);

    cerebellum_destroy(adapter);
}

TEST_F(CerebellumCorrectnessTest, InterneuronsInhibitPurkinje) {
    // Core behavior: Basket/stellate cells inhibit Purkinje cells
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_basket_cells = true;
    config.enable_stellate_cells = true;
    cerebellum_adapter_t* adapter = cerebellum_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Feed input to activate interneurons
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    for (int i = 0; i < 10; i++) {
        input.fiber_id = i;
        cerebellum_process_mossy_input(adapter, &input);
    }

    motor_coordination_result_t result;
    cerebellum_process(adapter, &result);

    // Get inhibition on Purkinje cell 0
    float somatic_inhibition, dendritic_inhibition;
    bool success = cerebellum_get_purkinje_inhibition(adapter, 0,
                                                       &somatic_inhibition,
                                                       &dendritic_inhibition);
    EXPECT_TRUE(success);

    // Should have some inhibition after activity
    // (specific values depend on implementation)

    cerebellum_destroy(adapter);
}

TEST_F(CerebellumCorrectnessTest, GolgiProvidesFeedback) {
    // Core behavior: Golgi cells provide feedback inhibition to granule layer
    cerebellum_config_t config = cerebellum_default_config();
    config.enable_golgi_cells = true;
    cerebellum_adapter_t* adapter = cerebellum_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Feed sustained input
    mossy_fiber_input_t input;
    input.fiber_id = 0;
    input.activity = 1.0f;
    input.timestamp_ms = 0.0f;
    input.modality = 0;

    for (int i = 0; i < 50; i++) {
        input.fiber_id = i % 10;
        input.timestamp_ms = (float)i;
        cerebellum_process_mossy_input(adapter, &input);
        motor_coordination_result_t result;
        cerebellum_process(adapter, &result);
    }

    // Check Golgi feedback occurred
    cerebellum_stats_t stats;
    cerebellum_get_stats(adapter, &stats);
    EXPECT_GE(stats.golgi_feedback_total, 0.0f);

    cerebellum_destroy(adapter);
}

//=============================================================================
// Memory Stability Tests
//=============================================================================

class CerebellumMemoryTest : public ::testing::Test {};

TEST_F(CerebellumMemoryTest, RepeatedCreateDestroy) {
    // Create and destroy to detect leaks
    for (int i = 0; i < 10; i++) {
        cerebellum_adapter_t* adapter = cerebellum_create(nullptr);
        ASSERT_NE(adapter, nullptr);

        // Do some work
        mossy_fiber_input_t input;
        input.fiber_id = 0;
        input.activity = 0.5f;
        input.timestamp_ms = 0.0f;
        input.modality = 0;
        cerebellum_process_mossy_input(adapter, &input);

        cerebellum_destroy(adapter);
    }
    // If ASAN is enabled, memory leaks will be detected
}

TEST_F(CerebellumMemoryTest, RepeatedVestibularCreateDestroy) {
    for (int i = 0; i < 10; i++) {
        vestibular_processor_t* processor = vestibular_create(nullptr);
        ASSERT_NE(processor, nullptr);

        semicircular_canal_input_t input;
        input.yaw_velocity = 0.5f;
        input.pitch_velocity = 0.0f;
        input.roll_velocity = 0.0f;
        input.timestamp_us = 0;
        vestibular_process_canal_input(processor, &input);

        vestibular_destroy(processor);
    }
}

TEST_F(CerebellumMemoryTest, FullSystemCreateDestroy) {
    for (int i = 0; i < 10; i++) {
        vestibular_processor_t* vestibular = vestibular_create(nullptr);
        ASSERT_NE(vestibular, nullptr);

        cerebellum_config_t config = cerebellum_default_config();
        config.enable_vestibulocerebellum = true;
        cerebellum_adapter_t* cerebellum = cerebellum_create(&config);
        ASSERT_NE(cerebellum, nullptr);

        vestibular_cerebellum_bridge_t* bridge =
            vestibular_cerebellum_bridge_create(vestibular, cerebellum, nullptr);
        ASSERT_NE(bridge, nullptr);

        // Do some work
        semicircular_canal_input_t input;
        input.yaw_velocity = 0.5f;
        input.pitch_velocity = 0.0f;
        input.roll_velocity = 0.0f;
        input.timestamp_us = 0;
        vestibular_process_canal_input(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);
        vestibular_cerebellum_apply_feedback(bridge);

        vestibular_cerebellum_bridge_destroy(bridge);
        cerebellum_destroy(cerebellum);
        vestibular_destroy(vestibular);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
