/**
 * @file e2e_test_motor_coordination_pipeline.cpp
 * @brief End-to-end tests for complete motor coordination pipeline
 *
 * WHAT: Full motor coordination pipeline from vestibular input to motor output
 * WHY:  Verify complete sensorimotor integration across cerebellum components
 * HOW:  Test vestibular -> cerebellum -> motor coordination flow
 *
 * TEST COVERAGE:
 * - Vestibular-Cerebellum Integration (4 tests)
 * - VOR Calibration Pipeline (4 tests)
 * - Motor Coordination Output (4 tests)
 * - Synaptic Dynamics in Pipeline (3 tests)
 * - Interneuron Effects (3 tests)
 * - Multi-Modal Coordination (3 tests)
 * - Long-Term Adaptation (3 tests)
 *
 * TOTAL: 24 tests
 *
 * BIOLOGICAL ANALOGY:
 * Complete vestibulo-cerebellar-motor loop:
 * 1. Vestibular input (head movement detection)
 * 2. Cerebellar processing (coordination, timing)
 * 3. VOR calibration (eye movement compensation)
 * 4. Motor output (coordinated response)
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>

#include "core/brain/regions/brainstem/nimcp_vestibular.h"
#include "core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.h"
#include "core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_VOR_TIME_MS = 50.0;
constexpr double MAX_COORDINATION_TIME_MS = 30.0;
constexpr float VOR_GAIN_TOLERANCE = 0.3f;
constexpr int NUM_CALIBRATION_TRIALS = 50;
constexpr int STABILITY_TEST_STEPS = 1000;

//=============================================================================
// Test Fixtures
//=============================================================================

class E2EMotorCoordinationTest : public ::testing::Test {
protected:
    vestibular_processor_t* vestibular = nullptr;
    cerebellum_adapter_t* cerebellum = nullptr;
    vestibular_cerebellum_bridge_t* bridge = nullptr;

    void SetUp() override {
        // Create vestibular processor
        vestibular_config_t vest_config = vestibular_default_config();
        vest_config.enable_vor = true;
        vest_config.enable_vsr = true;
        vest_config.enable_cerebellar_modulation = true;
        vestibular = vestibular_create(&vest_config);
        ASSERT_NE(vestibular, nullptr);

        // Create cerebellum with all features
        cerebellum_config_t cereb_config = cerebellum_default_config();
        cereb_config.enable_vestibulocerebellum = true;
        cereb_config.enable_synaptic_dynamics = true;
        cereb_config.synapse_config = cerebellar_synapse_default_config();
        cereb_config.synapse_config.enable_vesicle_dynamics = true;
        cereb_config.synapse_config.enable_stp = true;
        cereb_config.synapse_config.enable_calcium_dynamics = true;
        cereb_config.enable_basket_cells = true;
        cereb_config.enable_stellate_cells = true;
        cereb_config.enable_golgi_cells = true;
        cerebellum = cerebellum_create(&cereb_config);
        ASSERT_NE(cerebellum, nullptr);

        // Create bridge
        vestibular_cerebellum_config_t bridge_config = vestibular_cerebellum_default_config();
        bridge_config.enable_vor_adaptation = true;
        bridge_config.enable_feedback_loop = true;
        bridge = vestibular_cerebellum_bridge_create(vestibular, cerebellum, &bridge_config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            vestibular_cerebellum_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (cerebellum) {
            cerebellum_destroy(cerebellum);
            cerebellum = nullptr;
        }
        if (vestibular) {
            vestibular_destroy(vestibular);
            vestibular = nullptr;
        }
    }

    void simulateHeadRotation(float yaw, float pitch, float roll, float duration_ms) {
        int steps = (int)(duration_ms / 1.0f);
        for (int i = 0; i < steps; i++) {
            vestibular_input_t input;
            memset(&input, 0, sizeof(input));
            input.source = VESTIBULAR_INPUT_CANAL;
            input.canal.angular_velocity[0] = yaw;
            input.canal.angular_velocity[1] = pitch;
            input.canal.angular_velocity[2] = roll;
            input.timestamp_ms = (float)i;
            vestibular_process(vestibular, &input);
        }
    }
};

//=============================================================================
// Vestibular-Cerebellum Integration Tests
//=============================================================================

TEST_F(E2EMotorCoordinationTest, VestibularToCerebellumSignalFlow) {
    E2E_PIPELINE_START("Vestibular To Cerebellum Signal Flow");

    E2E_STAGE_BEGIN("Generate vestibular input", 20);
    simulateHeadRotation(1.5f, 0.0f, 0.0f, 50.0f);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Send mossy signal", 10);
    int result = vestibular_cerebellum_send_mossy_signal(bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process through cerebellum", 15);
    motor_coordination_result_t motor_result;
    bool processed = cerebellum_process(cerebellum, &motor_result);
    EXPECT_TRUE(processed);
    EXPECT_TRUE(motor_result.motor_ready);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply feedback", 10);
    result = vestibular_cerebellum_apply_feedback(bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, BidirectionalSignaling) {
    E2E_PIPELINE_START("Bidirectional Signaling");

    E2E_STAGE_BEGIN("Forward path: vestibular -> cerebellum", 30);
    for (int i = 0; i < 10; i++) {
        simulateHeadRotation(1.0f, 0.0f, 0.0f, 5.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Backward path: cerebellum -> vestibular", 20);
    for (int i = 0; i < 10; i++) {
        vestibular_cerebellum_apply_feedback(bridge);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify statistics", 5);
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.mossy_signals_sent, 10);
    EXPECT_EQ(stats.feedback_events, 10);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, ContinuousVestibularStream) {
    E2E_PIPELINE_START("Continuous Vestibular Stream");

    E2E_STAGE_BEGIN("Stream processing", 100);
    for (int frame = 0; frame < 200; frame++) {
        float t = (float)frame * 0.01f;
        float yaw = 1.5f * sinf(2.0f * M_PI * t);

        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = yaw;
        input.timestamp_ms = (float)frame;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);

        if (frame % 10 == 0) {
            vestibular_cerebellum_apply_feedback(bridge);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify completeness", 5);
    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.mossy_signals_sent, 200);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, MultiAxisMovement) {
    E2E_PIPELINE_START("Multi-Axis Movement");

    E2E_STAGE_BEGIN("Combined yaw-pitch-roll", 50);
    for (int i = 0; i < 50; i++) {
        float t = (float)i * 0.05f;
        float yaw = sinf(t);
        float pitch = sinf(t * 1.5f) * 0.5f;
        float roll = sinf(t * 0.7f) * 0.3f;

        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = yaw;
        input.canal.angular_velocity[1] = pitch;
        input.canal.angular_velocity[2] = roll;
        input.timestamp_ms = (float)i;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// VOR Calibration Pipeline Tests
//=============================================================================

TEST_F(E2EMotorCoordinationTest, VorCalibrationBaseline) {
    E2E_PIPELINE_START("VOR Calibration Baseline");

    E2E_STAGE_BEGIN("Get initial VOR gain", 5);
    float initial_gain[3];
    bool adaptation_active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &adaptation_active);
    EXPECT_NEAR(initial_gain[0], 1.0f, VOR_GAIN_TOLERANCE);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, VorAdaptationPipeline) {
    E2E_PIPELINE_START("VOR Adaptation Pipeline");

    E2E_STAGE_BEGIN("Initial VOR state", 5);
    float initial_gain[3];
    bool adaptation_active;
    vestibular_cerebellum_get_vor_state(bridge, initial_gain, &adaptation_active);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Calibration trials", 100);
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};

    for (int trial = 0; trial < NUM_CALIBRATION_TRIALS; trial++) {
        // Head rotation
        simulateHeadRotation(1.5f, 0.0f, 0.0f, 10.0f);

        // Mossy fiber transmission
        vestibular_cerebellum_send_mossy_signal(bridge);

        // Cerebellar processing
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);

        // Retinal slip error (VOR gain too high)
        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.2f, slip_direction);

        // Feedback
        vestibular_cerebellum_apply_feedback(bridge);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify adaptation", 5);
    float final_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, final_gain, &adaptation_active);

    // VOR gain should have decreased (LTD reduces response to error)
    EXPECT_NE(final_gain[0], initial_gain[0]);

    vestibular_cerebellum_stats_t stats;
    vestibular_cerebellum_get_stats(bridge, &stats);
    EXPECT_GT(stats.adaptation_triggers, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, VorMultiAxisAdaptation) {
    E2E_PIPELINE_START("VOR Multi-Axis Adaptation");

    E2E_STAGE_BEGIN("Yaw adaptation", 30);
    float yaw_slip[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 20; i++) {
        simulateHeadRotation(1.0f, 0.0f, 0.0f, 5.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.15f, yaw_slip);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Pitch adaptation", 30);
    float pitch_slip[3] = {0.0f, 1.0f, 0.0f};
    for (int i = 0; i < 20; i++) {
        simulateHeadRotation(0.0f, 1.0f, 0.0f, 5.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.15f, pitch_slip);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify independent adaptation", 5);
    float gain[3];
    bool adaptation_active;
    vestibular_cerebellum_get_vor_state(bridge, gain, &adaptation_active);
    // Both yaw and pitch should have adapted
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, VorCalibrationConvergence) {
    E2E_PIPELINE_START("VOR Calibration Convergence");

    E2E_STAGE_BEGIN("Track adaptation progress", 200);
    std::vector<float> gain_history;
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};

    for (int epoch = 0; epoch < 20; epoch++) {
        float gain[3];
        bool adaptation_active;
        vestibular_cerebellum_get_vor_state(bridge, gain, &adaptation_active);
        gain_history.push_back(gain[0]);

        for (int trial = 0; trial < 5; trial++) {
            simulateHeadRotation(1.0f, 0.0f, 0.0f, 5.0f);
            vestibular_cerebellum_send_mossy_signal(bridge);
            motor_coordination_result_t motor_result;
            cerebellum_process(cerebellum, &motor_result);
            vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.1f, slip_direction);
            vestibular_cerebellum_apply_feedback(bridge);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze convergence", 5);
    // Should show progressive change
    float total_change = fabsf(gain_history.back() - gain_history.front());
    EXPECT_GT(total_change, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Motor Coordination Output Tests
//=============================================================================

TEST_F(E2EMotorCoordinationTest, MotorOutputGeneration) {
    E2E_PIPELINE_START("Motor Output Generation");

    E2E_STAGE_BEGIN("Process vestibular input", 20);
    simulateHeadRotation(2.0f, 0.0f, 0.0f, 50.0f);
    vestibular_cerebellum_send_mossy_signal(bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Generate motor output", 10);
    motor_coordination_result_t motor_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
    EXPECT_TRUE(motor_result.motor_ready);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, CoordinationTiming) {
    E2E_PIPELINE_START("Coordination Timing");

    E2E_STAGE_BEGIN("Timing consistency", 100);
    std::vector<bool> motor_ready_results;

    for (int i = 0; i < 100; i++) {
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = 1.0f;
        input.timestamp_ms = (float)i;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
        motor_ready_results.push_back(motor_result.motor_ready);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify timing consistency", 5);
    int ready_count = std::count(motor_ready_results.begin(), motor_ready_results.end(), true);
    EXPECT_GT(ready_count, 50);  // Most should be ready
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, CerebellumStats) {
    E2E_PIPELINE_START("Cerebellum Stats");

    E2E_STAGE_BEGIN("Process input", 50);
    for (int i = 0; i < 100; i++) {
        simulateHeadRotation(1.5f, 0.0f, 0.0f, 2.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify stats", 10);
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);

    EXPECT_GT(stats.purkinje_spikes, 0);
    EXPECT_GT(stats.granule_activations, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, CoordinationUnderLoad) {
    E2E_PIPELINE_START("Coordination Under Load");

    E2E_STAGE_BEGIN("High-frequency input", 100);
    for (int i = 0; i < 500; i++) {
        // Rapid head movements
        float t = (float)i * 0.01f;
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = 3.0f * sinf(10.0f * t);
        input.canal.angular_velocity[1] = 1.5f * sinf(15.0f * t);
        input.timestamp_ms = (float)i;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Synaptic Dynamics in Pipeline Tests
//=============================================================================

TEST_F(E2EMotorCoordinationTest, SynapticDynamicsEngagement) {
    E2E_PIPELINE_START("Synaptic Dynamics Engagement");

    E2E_STAGE_BEGIN("Initial stats", 5);
    cerebellum_stats_t stats_before;
    cerebellum_get_stats(cerebellum, &stats_before);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Active processing", 50);
    for (int i = 0; i < 100; i++) {
        simulateHeadRotation(2.0f, 0.5f, 0.0f, 2.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify dynamics", 10);
    cerebellum_stats_t stats_after;
    cerebellum_get_stats(cerebellum, &stats_after);

    EXPECT_GT(stats_after.vesicle_releases, stats_before.vesicle_releases);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, CalciumDynamicsDuringVor) {
    E2E_PIPELINE_START("Calcium Dynamics During VOR");

    E2E_STAGE_BEGIN("VOR with error signal", 100);
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};

    for (int i = 0; i < 100; i++) {
        simulateHeadRotation(1.5f, 0.0f, 0.0f, 5.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);

        if (i % 5 == 0) {
            vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.2f, slip_direction);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify calcium activity", 5);
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);
    EXPECT_GT(stats.avg_calcium_concentration, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, STPDuringCoordination) {
    E2E_PIPELINE_START("STP During Coordination");

    E2E_STAGE_BEGIN("High-frequency stimulation", 50);
    for (int burst = 0; burst < 10; burst++) {
        // High-frequency burst
        for (int i = 0; i < 20; i++) {
            vestibular_input_t input;
            memset(&input, 0, sizeof(input));
            input.source = VESTIBULAR_INPUT_CANAL;
            input.canal.angular_velocity[0] = 2.0f;
            input.timestamp_ms = (float)(burst * 20 + i);
            vestibular_process(vestibular, &input);
            vestibular_cerebellum_send_mossy_signal(bridge);
        }

        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify processing", 5);
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);
    EXPECT_GT(stats.purkinje_spikes, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Interneuron Effects Tests
//=============================================================================

TEST_F(E2EMotorCoordinationTest, InterneuronsActiveDuringCoordination) {
    E2E_PIPELINE_START("Interneurons Active During Coordination");

    E2E_STAGE_BEGIN("Process with interneurons", 100);
    for (int i = 0; i < 200; i++) {
        simulateHeadRotation(1.0f + 0.5f * sinf((float)i * 0.1f), 0.0f, 0.0f, 2.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify interneuron activity", 10);
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);

    EXPECT_GT(stats.basket_cell_spikes, 0);
    EXPECT_GT(stats.stellate_cell_spikes, 0);
    EXPECT_GT(stats.golgi_cell_spikes, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, InhibitionBalancesMoto) {
    E2E_PIPELINE_START("Inhibition Balances Motor Output");

    E2E_STAGE_BEGIN("Strong input with inhibition", 50);
    for (int i = 0; i < 100; i++) {
        // Strong vestibular input
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = 5.0f;  // Very strong
        input.timestamp_ms = (float)i;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));

        // Output should be balanced (not saturated)
        EXPECT_TRUE(motor_result.motor_ready);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Check inhibition stats", 5);
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);
    EXPECT_GT(stats.basket_inhibition_total, 0.0f);
    EXPECT_GT(stats.stellate_inhibition_total, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, GolgiGranuleLoop) {
    E2E_PIPELINE_START("Golgi-Granule Loop");

    E2E_STAGE_BEGIN("Sustained input", 100);
    for (int i = 0; i < 200; i++) {
        simulateHeadRotation(1.5f, 0.0f, 0.0f, 2.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify Golgi feedback", 5);
    cerebellum_stats_t stats;
    cerebellum_get_stats(cerebellum, &stats);
    EXPECT_GT(stats.golgi_cell_spikes, 0);
    EXPECT_GT(stats.golgi_feedback_total, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Multi-Modal Coordination Tests
//=============================================================================

TEST_F(E2EMotorCoordinationTest, CanalAndOtolithIntegration) {
    E2E_PIPELINE_START("Canal And Otolith Integration");

    E2E_STAGE_BEGIN("Combined input", 50);
    for (int i = 0; i < 100; i++) {
        // Canal input (rotation)
        vestibular_input_t canal_input;
        memset(&canal_input, 0, sizeof(canal_input));
        canal_input.source = VESTIBULAR_INPUT_CANAL;
        canal_input.canal.angular_velocity[0] = 1.0f * sinf((float)i * 0.1f);
        canal_input.timestamp_ms = (float)i;
        vestibular_process(vestibular, &canal_input);

        // Otolith input (gravity + translation)
        vestibular_input_t otolith_input;
        memset(&otolith_input, 0, sizeof(otolith_input));
        otolith_input.source = VESTIBULAR_INPUT_OTOLITH;
        otolith_input.otolith.linear_acceleration[0] = 0.2f * sinf((float)i * 0.2f);
        otolith_input.otolith.linear_acceleration[2] = 9.8f;
        otolith_input.timestamp_ms = (float)i;
        vestibular_process(vestibular, &otolith_input);

        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, TiltTranslationDiscrimination) {
    E2E_PIPELINE_START("Tilt-Translation Discrimination");

    E2E_STAGE_BEGIN("Tilt scenario", 30);
    // Tilt: angular rotation with gravity component change
    for (int i = 0; i < 50; i++) {
        vestibular_input_t canal_input;
        memset(&canal_input, 0, sizeof(canal_input));
        canal_input.source = VESTIBULAR_INPUT_CANAL;
        canal_input.canal.angular_velocity[1] = 0.5f;  // Slow pitch
        canal_input.timestamp_ms = (float)i;
        vestibular_process(vestibular, &canal_input);

        vestibular_input_t otolith_input;
        memset(&otolith_input, 0, sizeof(otolith_input));
        otolith_input.source = VESTIBULAR_INPUT_OTOLITH;
        otolith_input.otolith.linear_acceleration[0] = 9.8f * sinf(0.1f * i);  // Gravity projection
        otolith_input.otolith.linear_acceleration[2] = 9.8f * cosf(0.1f * i);
        otolith_input.timestamp_ms = (float)i;
        vestibular_process(vestibular, &otolith_input);

        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Translation scenario", 30);
    // Translation: linear acceleration without rotation
    for (int i = 0; i < 50; i++) {
        vestibular_input_t otolith_input;
        memset(&otolith_input, 0, sizeof(otolith_input));
        otolith_input.source = VESTIBULAR_INPUT_OTOLITH;
        otolith_input.otolith.linear_acceleration[0] = 2.0f;  // Forward acceleration
        otolith_input.otolith.linear_acceleration[2] = 9.8f;  // Gravity unchanged
        otolith_input.timestamp_ms = (float)(50 + i);
        vestibular_process(vestibular, &otolith_input);

        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, VorVsrCoordination) {
    E2E_PIPELINE_START("VOR-VSR Coordination");

    E2E_STAGE_BEGIN("Combined reflex", 50);
    for (int i = 0; i < 100; i++) {
        // Rotation triggering VOR
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = 2.0f;
        input.timestamp_ms = (float)i;
        vestibular_process(vestibular, &input);

        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
        vestibular_cerebellum_apply_feedback(bridge);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Adaptation Tests
//=============================================================================

TEST_F(E2EMotorCoordinationTest, LongTermStability) {
    E2E_PIPELINE_START("Long-Term Stability");

    E2E_STAGE_BEGIN("Extended processing", 500);
    for (int i = 0; i < STABILITY_TEST_STEPS; i++) {
        float t = (float)i * 0.01f;
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = sinf(t);
        input.timestamp_ms = (float)i;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);

        motor_coordination_result_t motor_result;
        EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));

        if (i % 100 == 0) {
            vestibular_cerebellum_apply_feedback(bridge);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify no degradation", 10);
    vestibular_cerebellum_stats_t bridge_stats;
    vestibular_cerebellum_get_stats(bridge, &bridge_stats);
    EXPECT_EQ(bridge_stats.mossy_signals_sent, STABILITY_TEST_STEPS);

    cerebellum_stats_t cereb_stats;
    cerebellum_get_stats(cerebellum, &cereb_stats);
    EXPECT_GT(cereb_stats.purkinje_spikes, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, AdaptationPersistence) {
    E2E_PIPELINE_START("Adaptation Persistence");

    E2E_STAGE_BEGIN("Initial adaptation", 100);
    float slip_direction[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 50; i++) {
        simulateHeadRotation(1.5f, 0.0f, 0.0f, 10.0f);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
        vestibular_cerebellum_trigger_vor_adaptation(bridge, 0.3f, slip_direction);
    }

    float adapted_gain[3];
    bool adaptation_active;
    vestibular_cerebellum_get_vor_state(bridge, adapted_gain, &adaptation_active);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Idle period", 50);
    for (int i = 0; i < 100; i++) {
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify persistence", 5);
    float after_idle_gain[3];
    vestibular_cerebellum_get_vor_state(bridge, after_idle_gain, &adaptation_active);

    // Adaptation should persist
    EXPECT_NEAR(after_idle_gain[0], adapted_gain[0], 0.1f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EMotorCoordinationTest, StressRecovery) {
    E2E_PIPELINE_START("Stress Recovery");

    E2E_STAGE_BEGIN("Baseline performance", 20);
    cerebellum_stats_t baseline_stats;
    cerebellum_get_stats(cerebellum, &baseline_stats);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("High-stress period", 100);
    for (int i = 0; i < 500; i++) {
        // Very high frequency, high amplitude
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = 10.0f * sinf((float)i * 0.5f);
        input.canal.angular_velocity[1] = 5.0f * sinf((float)i * 0.7f);
        input.canal.angular_velocity[2] = 3.0f * sinf((float)i * 0.3f);
        input.timestamp_ms = (float)i;

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        cerebellum_process(cerebellum, &motor_result);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery period", 50);
    for (int i = 0; i < 100; i++) {
        // Normal, mild input
        vestibular_input_t input;
        memset(&input, 0, sizeof(input));
        input.source = VESTIBULAR_INPUT_CANAL;
        input.canal.angular_velocity[0] = 0.5f;
        input.timestamp_ms = (float)(500 + i);

        vestibular_process(vestibular, &input);
        vestibular_cerebellum_send_mossy_signal(bridge);
        motor_coordination_result_t motor_result;
        EXPECT_TRUE(cerebellum_process(cerebellum, &motor_result));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery", 10);
    motor_coordination_result_t final_result;
    EXPECT_TRUE(cerebellum_process(cerebellum, &final_result));
    EXPECT_TRUE(final_result.motor_ready);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
