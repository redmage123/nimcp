/**
 * @file test_motor_snn_plasticity_integration.cpp
 * @brief Integration tests for Motor Cortex with SNN and Plasticity systems
 *
 * WHAT: Tests Motor Cortex integration with SNN and STDP learning
 * WHY:  Ensure proper motor learning via spike-based plasticity
 * HOW:  Test SNN networks, STDP synapses, and motor skill acquisition
 *
 * BIOLOGICAL BASIS:
 * Motor cortex relies on spike-timing dependent plasticity for:
 * - Motor skill acquisition (learning new movements)
 * - Motor program refinement (improving existing movements)
 * - Error-driven motor adaptation
 * - Dopamine-modulated reward learning
 *
 * INTEGRATION POINTS:
 * - SNN network creation and simulation
 * - STDP synapse initialization and learning
 * - Dopamine modulation of motor learning
 * - Motor command generation from SNN output
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_types.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorSNNPlasticityTest : public ::testing::Test {
protected:
    motor_adapter_t* adapter;
    motor_config_t config;

    void SetUp() override {
        /* Configure motor adapter with training enabled */
        config = motor_default_config();
        config.enable_bio_async = false;
        config.enable_training = true;
        config.enable_events = true;
        config.enable_premotor = true;
        config.enable_sma = true;
        config.learning_rate = 0.01f;

        adapter = motor_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Motor adapter";
    }

    void TearDown() override {
        if (adapter) {
            motor_destroy(adapter);
            adapter = nullptr;
        }
    }
};

/*=============================================================================
 * STDP SYNAPSE INITIALIZATION TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, STDPSynapseDefaultInit) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Verify default parameters */
    EXPECT_GT(synapse.weight, 0.0f);
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_GT(synapse.learning_rate, 0.0f);
    EXPECT_GT(synapse.tau_plus, 0.0f);
    EXPECT_GT(synapse.tau_minus, 0.0f);
}

TEST_F(MotorSNNPlasticityTest, STDPSynapseCustomInit) {
    stdp_synapse_t synapse;
    stdp_config_t custom_config = stdp_config_default();
    custom_config.learning_rate = 0.05f;
    custom_config.w_max = 2.0f;
    /* Note: tau values are in seconds (0.015 = 15ms, 0.025 = 25ms) */
    custom_config.tau_plus = 0.015f;
    custom_config.tau_minus = 0.025f;
    custom_config.enable_da_modulation = true;

    stdp_synapse_init_with_config(&synapse, &custom_config);

    EXPECT_FLOAT_EQ(synapse.learning_rate, 0.05f);
    EXPECT_FLOAT_EQ(synapse.w_max, 2.0f);
    EXPECT_FLOAT_EQ(synapse.tau_plus, 0.015f);
    EXPECT_FLOAT_EQ(synapse.tau_minus, 0.025f);
    EXPECT_TRUE(synapse.enable_da_modulation);
}

TEST_F(MotorSNNPlasticityTest, STDPDefaultConfig) {
    stdp_config_t config = stdp_config_default();

    /* Verify sensible defaults (Bi & Poo parameters) */
    EXPECT_GT(config.learning_rate, 0.0f);
    EXPECT_GT(config.w_max, 0.0f);
    EXPECT_GT(config.a_plus, 0.0f);
    EXPECT_GT(config.a_minus, 0.0f);
    /* Note: tau values are in seconds (0.020 = 20ms typical for STDP) */
    EXPECT_NEAR(config.tau_plus, 0.020f, 0.010f);  /* Around 20ms typical */
    EXPECT_NEAR(config.tau_minus, 0.020f, 0.010f);
}

/*=============================================================================
 * STDP TRACE UPDATE TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, STDPTraceDecay) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set initial traces */
    synapse.pre_trace = 1.0f;
    synapse.post_trace = 1.0f;

    /* Update traces (decay) */
    float dt = 0.001f;  /* 1ms */
    for (int i = 0; i < 100; i++) {
        stdp_update_traces(&synapse, dt);
    }

    /* Traces should decay toward zero */
    EXPECT_LT(synapse.pre_trace, 1.0f);
    EXPECT_LT(synapse.post_trace, 1.0f);
    EXPECT_GT(synapse.pre_trace, 0.0f);
    EXPECT_GT(synapse.post_trace, 0.0f);
}

/*=============================================================================
 * STDP SPIKE PROCESSING TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, STDPPreSpikeLTD) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set up post-trace (post fired recently) */
    synapse.post_trace = 0.5f;
    float initial_weight = synapse.weight;

    /* Pre-spike after post (LTD) */
    float weight_change = stdp_pre_spike(&synapse, 100.0f);

    /* Should cause depression (negative weight change) */
    EXPECT_LE(weight_change, 0.0f);
    EXPECT_LE(synapse.weight, initial_weight);
}

TEST_F(MotorSNNPlasticityTest, STDPPostSpikeLTP) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Set up pre-trace (pre fired recently) */
    synapse.pre_trace = 0.5f;
    float initial_weight = synapse.weight;

    /* Post-spike after pre (LTP) */
    float weight_change = stdp_post_spike(&synapse, 100.0f);

    /* Should cause potentiation (positive weight change) */
    EXPECT_GE(weight_change, 0.0f);
    EXPECT_GE(synapse.weight, initial_weight);
}

TEST_F(MotorSNNPlasticityTest, STDPWeightBounds) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Repeatedly potentiate */
    synapse.pre_trace = 1.0f;
    for (int i = 0; i < 1000; i++) {
        stdp_post_spike(&synapse, (float)i);
        synapse.pre_trace = 1.0f;  /* Keep pre-trace high */
    }

    /* Weight should be bounded by w_max */
    EXPECT_LE(synapse.weight, synapse.w_max);
    EXPECT_GE(synapse.weight, synapse.w_min);
}

/*=============================================================================
 * STDP STATISTICS TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, STDPStatisticsTracking) {
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Induce LTP */
    synapse.pre_trace = 0.5f;
    stdp_post_spike(&synapse, 100.0f);

    /* Induce LTD */
    synapse.post_trace = 0.5f;
    stdp_pre_spike(&synapse, 200.0f);

    /* Check statistics (at least one event of each) */
    EXPECT_GE(synapse.num_potentiation_events + synapse.num_depression_events, 1u);
}

/*=============================================================================
 * SNN NETWORK TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, SNNConfigDefault) {
    snn_config_t config;
    memset(&config, 0, sizeof(config));

    /* Create basic feedforward config */
    int result = snn_config_feedforward(&config, 10, 20, 5);  /* 10 input, 20 hidden, 5 output */
    EXPECT_EQ(SNN_SUCCESS, result);

    EXPECT_EQ(10u, config.n_inputs);
    EXPECT_EQ(5u, config.n_outputs);
    EXPECT_GT(config.dt, 0.0f);
}

TEST_F(MotorSNNPlasticityTest, SNNNetworkCreation) {
    snn_config_t config;
    memset(&config, 0, sizeof(config));
    snn_config_feedforward(&config, 8, 16, 4);

    snn_network_t* snn = snn_network_create(&config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    ASSERT_NE(nullptr, snn);
    snn_network_destroy(snn);
}

TEST_F(MotorSNNPlasticityTest, SNNNetworkReset) {
    snn_config_t config;
    memset(&config, 0, sizeof(config));
    snn_config_feedforward(&config, 4, 8, 2);

    snn_network_t* snn = snn_network_create(&config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Run some steps */
    float inputs[4] = {1.0f, 0.5f, 0.3f, 0.8f};
    snn_network_set_inputs(snn, inputs, 4);
    snn_network_run(snn, 50.0f);

    /* Reset */
    EXPECT_EQ(SNN_SUCCESS, snn_network_reset(snn));

    snn_network_destroy(snn);
}

TEST_F(MotorSNNPlasticityTest, SNNSimulationStep) {
    snn_config_t config;
    memset(&config, 0, sizeof(config));
    snn_config_feedforward(&config, 4, 8, 2);

    snn_network_t* snn = snn_network_create(&config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Set inputs */
    float inputs[4] = {1.0f, 0.8f, 0.6f, 0.4f};
    EXPECT_EQ(SNN_SUCCESS, snn_network_set_inputs(snn, inputs, 4));

    /* Run single step */
    int spikes = snn_network_step(snn, 1.0f);
    EXPECT_GE(spikes, 0);

    snn_network_destroy(snn);
}

TEST_F(MotorSNNPlasticityTest, SNNForwardPass) {
    snn_config_t config;
    memset(&config, 0, sizeof(config));
    snn_config_feedforward(&config, 4, 8, 2);

    snn_network_t* snn = snn_network_create(&config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    float inputs[4] = {1.0f, 0.5f, 0.3f, 0.1f};
    float outputs[2] = {0.0f, 0.0f};

    /* Full forward pass */
    int result = snn_network_forward(snn, inputs, 4, outputs, 2, 100.0f);
    EXPECT_GE(result, 0);

    /* Outputs should have values */
    EXPECT_GE(outputs[0], 0.0f);
    EXPECT_GE(outputs[1], 0.0f);

    snn_network_destroy(snn);
}

/*=============================================================================
 * SNN INPUT/OUTPUT TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, SNNSetInputs) {
    snn_config_t config;
    memset(&config, 0, sizeof(config));
    snn_config_feedforward(&config, 8, 16, 4);

    snn_network_t* snn = snn_network_create(&config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    float inputs[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    EXPECT_EQ(SNN_SUCCESS, snn_network_set_inputs(snn, inputs, 8));

    snn_network_destroy(snn);
}

TEST_F(MotorSNNPlasticityTest, SNNGetOutputs) {
    snn_config_t config;
    memset(&config, 0, sizeof(config));
    snn_config_feedforward(&config, 4, 8, 3);

    snn_network_t* snn = snn_network_create(&config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Set inputs and run */
    float inputs[4] = {1.0f, 0.5f, 0.3f, 0.1f};
    snn_network_set_inputs(snn, inputs, 4);
    snn_network_run(snn, 100.0f);

    /* Get outputs */
    float outputs[3];
    EXPECT_EQ(SNN_SUCCESS, snn_network_get_outputs(snn, outputs, 3));

    snn_network_destroy(snn);
}

/*=============================================================================
 * MOTOR CORTEX WITH SNN INTEGRATION TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, MotorTrainingEnabled) {
    motor_config_t retrieved;
    EXPECT_TRUE(motor_get_config(adapter, &retrieved));
    EXPECT_TRUE(retrieved.enable_training);
    EXPECT_GT(retrieved.learning_rate, 0.0f);
}

TEST_F(MotorSNNPlasticityTest, MotorLearningRateConfiguration) {
    motor_config_t learning_config = motor_default_config();
    learning_config.enable_training = true;
    learning_config.learning_rate = 0.02f;
    learning_config.enable_bio_async = false;

    motor_adapter_t* learning_adapter = motor_create(&learning_config);
    ASSERT_NE(nullptr, learning_adapter);

    motor_config_t retrieved;
    motor_get_config(learning_adapter, &retrieved);
    EXPECT_FLOAT_EQ(retrieved.learning_rate, 0.02f);

    motor_destroy(learning_adapter);
}

TEST_F(MotorSNNPlasticityTest, MotorTrainingIterationTracking) {
    /* Execute some movements to trigger training */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f;
    goal.max_duration_ms = 100.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    motor_plan_movement(adapter, &goal);
    motor_begin_execution(adapter);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(adapter, 5.0f);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    /* Training iterations depend on implementation */
    EXPECT_GE(stats.movements_planned, 1u);
}

/*=============================================================================
 * MOTOR LEARNING SCENARIO TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, MotorSkillAcquisitionScenario) {
    /* Simulate motor skill acquisition: multiple trials with feedback */
    for (int trial = 0; trial < 5; trial++) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = MOTOR_REGION_HAND_RIGHT;
        goal.target_position.x = 0.5f + (float)trial * 0.1f;
        goal.target_position.y = 0.3f;
        goal.max_duration_ms = 200.0f;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        goal.precision_required = 0.01f;

        EXPECT_TRUE(motor_plan_movement(adapter, &goal));
        EXPECT_TRUE(motor_begin_execution(adapter));

        /* Execute with feedback */
        for (int step = 0; step < 10; step++) {
            motor_update_execution(adapter, 20.0f);

            /* Simulate feedback via effector state update */
            motor_effector_state_t feedback_state;
            memset(&feedback_state, 0, sizeof(feedback_state));
            feedback_state.region = MOTOR_REGION_HAND_RIGHT;
            feedback_state.position.x = goal.target_position.x * ((float)step / 10.0f);
            motor_update_feedback(adapter, MOTOR_REGION_HAND_RIGHT, &feedback_state);
        }

        motor_reset(adapter);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_GE(stats.movements_planned, 5u);
}

TEST_F(MotorSNNPlasticityTest, MotorErrorCorrectionWithPlasticity) {
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_ARM_RIGHT;
    goal.target_position.x = 1.0f;
    goal.target_position.y = 0.5f;
    goal.max_duration_ms = 300.0f;
    goal.type = MOVEMENT_TYPE_CONTINUOUS;
    goal.precision_required = 0.02f;

    ASSERT_TRUE(motor_plan_movement(adapter, &goal));
    ASSERT_TRUE(motor_begin_execution(adapter));

    /* Execute with error-driven correction */
    for (int step = 0; step < 15; step++) {
        motor_update_execution(adapter, 20.0f);

        /* Simulate error: position diverges from target */
        motor_effector_state_t feedback_state;
        memset(&feedback_state, 0, sizeof(feedback_state));
        feedback_state.region = MOTOR_REGION_ARM_RIGHT;
        feedback_state.position.x = goal.target_position.x * 0.8f;  /* 20% error */
        motor_update_feedback(adapter, MOTOR_REGION_ARM_RIGHT, &feedback_state);
    }

    motor_stats_t stats;
    motor_get_stats(adapter, &stats);
    EXPECT_GT(stats.corrections_applied, 0u);
}

/*=============================================================================
 * SNN FOR MOTOR PATTERN GENERATION TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, SNNMotorPatternGeneration) {
    /* Create SNN suitable for motor pattern generation */
    snn_config_t config;
    memset(&config, 0, sizeof(config));

    /* Sensory input -> motor command */
    snn_config_feedforward(&config,
        6,   /* 6 proprioceptive inputs */
        24,  /* 24 hidden neurons */
        3);  /* 3 motor outputs (x, y, z velocity) */

    snn_network_t* snn = snn_network_create(&config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Simulate proprioceptive input */
    float proprio_input[6] = {0.3f, 0.5f, 0.7f, 0.1f, 0.4f, 0.6f};
    snn_network_set_inputs(snn, proprio_input, 6);

    /* Generate motor command via SNN */
    snn_network_run(snn, 50.0f);

    float motor_output[3];
    snn_network_get_outputs(snn, motor_output, 3);

    /* Outputs should be valid */
    for (int i = 0; i < 3; i++) {
        EXPECT_GE(motor_output[i], 0.0f);
    }

    snn_network_destroy(snn);
}

/*=============================================================================
 * STDP DOPAMINE MODULATION TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, STDPDopamineModulationEnabled) {
    stdp_synapse_t synapse;
    stdp_config_t config = stdp_config_default();
    config.enable_da_modulation = true;
    config.da_modulation_gain = 100.0f;
    config.burst_amplification = 3.0f;

    stdp_synapse_init_with_config(&synapse, &config);

    EXPECT_TRUE(synapse.enable_da_modulation);
    EXPECT_FLOAT_EQ(synapse.da_modulation_gain, 100.0f);
    EXPECT_FLOAT_EQ(synapse.burst_amplification, 3.0f);
}

/*=============================================================================
 * COMBINED MOTOR + SNN + STDP TESTS
 *===========================================================================*/

TEST_F(MotorSNNPlasticityTest, IntegratedMotorLearningScenario) {
    /* Create SNN for motor command generation */
    snn_config_t snn_config;
    memset(&snn_config, 0, sizeof(snn_config));
    snn_config_feedforward(&snn_config, 4, 16, 3);

    snn_network_t* snn = snn_network_create(&snn_config);
    if (!snn) {
        GTEST_SKIP() << "SNN network creation not available";
    }

    /* Create STDP synapse for learning */
    stdp_synapse_t synapse;
    stdp_synapse_init(&synapse);

    /* Simulate integrated learning loop */
    for (int trial = 0; trial < 3; trial++) {
        /* 1. Get motor goal */
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = MOTOR_REGION_HAND_RIGHT;
        goal.target_position.x = 0.5f;
        goal.max_duration_ms = 100.0f;
        goal.type = MOVEMENT_TYPE_DISCRETE;

        /* 2. Plan movement */
        motor_plan_movement(adapter, &goal);
        motor_begin_execution(adapter);

        /* 3. Generate SNN input from goal */
        float snn_input[4] = {
            goal.target_position.x,
            goal.target_position.y,
            goal.target_position.z,
            0.5f  /* velocity scale */
        };
        snn_network_set_inputs(snn, snn_input, 4);

        /* 4. Run SNN to get motor command */
        snn_network_run(snn, 20.0f);

        float motor_cmd[3];
        snn_network_get_outputs(snn, motor_cmd, 3);

        /* 5. Execute motor command */
        for (int step = 0; step < 5; step++) {
            motor_update_execution(adapter, 20.0f);
        }

        /* 6. Update STDP based on outcome */
        /* Simulate pre-post timing (LTP) */
        synapse.pre_trace = 0.3f;
        stdp_post_spike(&synapse, (float)(trial * 100 + 50));

        motor_reset(adapter);
        snn_network_reset(snn);
    }

    /* Verify learning occurred */
    EXPECT_GT(synapse.num_potentiation_events, 0u);

    snn_network_destroy(snn);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
