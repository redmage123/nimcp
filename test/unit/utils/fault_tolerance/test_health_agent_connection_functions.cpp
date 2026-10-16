/**
 * @file test_health_agent_connection_functions.cpp
 * @brief Unit tests for NIMCP Health Agent Connection Functions (Phase 2)
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Test health agent connection functions for cognitive modules
 * WHY:  Ensure connection functions correctly configure and store modules
 * HOW:  Test each connect function with valid inputs, NULL inputs, default configs
 */

#include <gtest/gtest.h>
#include <cstring>

// Health agent header (has its own extern "C" guard)
#include "utils/fault_tolerance/nimcp_health_agent.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health agent connection tests
 */
class HealthAgentConnectionTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 100;
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

//=============================================================================
// connect_failure_prediction Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectFailurePrediction_NullAgent) {
    failure_predictor_t* predictor = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(nullptr, predictor, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectFailurePrediction_NullPredictor_WithDefaults) {
    // Should succeed with NULL predictor - stores NULL and applies defaults
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectFailurePrediction_WithCustomConfig) {
    health_agent_prediction_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_failure_prediction = true;
    custom_config.prediction_threshold = 0.85f;
    custom_config.prediction_horizon_ms = 120000;
    custom_config.enable_preventive_action = false;
    custom_config.enable_trend_analysis = true;

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectFailurePrediction_MultipleConnects) {
    // Should allow reconnection (replacing previous connection)
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
}

//=============================================================================
// connect_metacognition Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectMetacognition_NullAgent) {
    metacognition_t* metacog = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(nullptr, metacog, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectMetacognition_NullModule_WithDefaults) {
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectMetacognition_WithCustomConfig) {
    health_agent_metacog_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_metacognition = true;
    custom_config.enable_confidence_calibration = false;
    custom_config.enable_degradation_detection = true;
    custom_config.degradation_threshold = 0.5f;
    custom_config.enable_self_diagnosis = false;

    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectMetacognition_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
}

//=============================================================================
// connect_ethics Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectEthics_NullAgent) {
    ethics_engine_t ethics = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_ethics(nullptr, ethics, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectEthics_NullEngine_WithDefaults) {
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectEthics_WithCustomConfig) {
    health_agent_ethics_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_ethics_evaluation = true;
    custom_config.enable_asimov_laws = false;
    custom_config.enable_mercy_directive = true;
    custom_config.enable_golden_rule = true;
    custom_config.ethics_override_threshold = 0.99f;

    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectEthics_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
}

//=============================================================================
// connect_emotion Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectEmotion_NullAgent) {
    emotional_system_t* emotion = nullptr;
    emotion_immune_bridge_t* bridge = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_emotion(nullptr, emotion, bridge, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectEmotion_NullModules_WithDefaults) {
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectEmotion_WithCustomConfig) {
    health_agent_emotion_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_emotion_awareness = true;
    custom_config.enable_emotion_reporting = false;
    custom_config.enable_stress_adjustment = true;
    custom_config.stress_threshold_multiplier = 2.0f;

    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectEmotion_WithBridgeOnly) {
    // Can connect just the bridge without the emotion system
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectEmotion_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
}

//=============================================================================
// connect_wellbeing Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectWellbeing_NullAgent) {
    wellbeing_monitor_t* wellbeing = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(nullptr, wellbeing, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectWellbeing_NullMonitor_WithDefaults) {
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectWellbeing_WithCustomConfig) {
    health_agent_wellbeing_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_wellbeing_monitoring = true;
    custom_config.enable_distress_detection = true;
    custom_config.enable_suffering_prevention = false;
    custom_config.distress_intervention_threshold = 0.5f;

    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectWellbeing_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
}

//=============================================================================
// connect_mental_health Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectMentalHealth_NullAgent) {
    mental_health_monitor_t* mental_health = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(nullptr, mental_health), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectMentalHealth_NullMonitor) {
    // No config parameter - just stores the pointer
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectMentalHealth_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
}

//=============================================================================
// connect_collective Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectCollective_NullAgent) {
    collective_cognition_t* collective = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_collective(nullptr, collective, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectCollective_NullModule_WithDefaults) {
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectCollective_WithCustomConfig) {
    health_agent_collective_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_collective_monitoring = true;
    custom_config.enable_consensus_decisions = false;
    custom_config.enable_swarm_immune = true;
    custom_config.consensus_threshold = 0.75f;
    custom_config.consensus_timeout_ms = 10000;

    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectCollective_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
}

//=============================================================================
// connect_rcog Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectRcog_NullAgent) {
    rcog_engine_t* rcog = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_rcog(nullptr, rcog, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectRcog_NullEngine_WithDefaults) {
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectRcog_WithCustomConfig) {
    health_agent_rcog_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_rcog_diagnosis = true;
    custom_config.enable_rcog_recovery_planning = false;
    custom_config.enable_imagination = true;
    custom_config.rcog_timeout_ms = 20000;
    custom_config.confidence_threshold = 0.8f;

    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectRcog_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
}

//=============================================================================
// connect_gpu Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectGpu_NullAgent) {
    gpu_health_monitor_t* gpu_health = nullptr;
    EXPECT_EQ(nimcp_health_agent_connect_gpu(nullptr, gpu_health, nullptr), -1);
}

TEST_F(HealthAgentConnectionTest, ConnectGpu_NullMonitor_WithDefaults) {
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectGpu_WithCustomConfig) {
    health_agent_gpu_config_t custom_config;
    memset(&custom_config, 0, sizeof(custom_config));
    custom_config.enable_gpu_monitoring = true;
    custom_config.enable_gpu_acceleration = false;
    custom_config.enable_tensor_validation = true;
    custom_config.enable_anomaly_detection = true;
    custom_config.gpu_check_interval_ms = 2000;

    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, &custom_config), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectGpu_MultipleConnects) {
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
}

//=============================================================================
// Cross-Module Connection Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectAllCognitiveModules) {
    // Connect all 9 cognitive modules in sequence
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectAllCognitiveModules_WithCustomConfigs) {
    // Connect all with custom configurations
    health_agent_prediction_config_t pred_cfg = {true, 0.9f, 30000, true, true};
    health_agent_metacog_config_t meta_cfg = {true, true, true, 0.4f, true};
    health_agent_ethics_config_t ethics_cfg = {true, true, true, true, 0.98f};
    health_agent_emotion_config_t emotion_cfg = {true, true, true, 1.2f};
    health_agent_wellbeing_config_t wellbeing_cfg = {true, true, true, 0.6f};
    health_agent_collective_config_t collective_cfg = {true, true, true, 0.7f, 3000};
    health_agent_rcog_config_t rcog_cfg = {true, true, true, 15000, 0.75f};
    health_agent_gpu_config_t gpu_cfg = {true, true, true, true, true, true, 500u, 70.0f, 85.0f, 0.8f, 0.95f};

    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &pred_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, &meta_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, &ethics_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, &emotion_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, &wellbeing_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &collective_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, &rcog_cfg), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, &gpu_cfg), 0);
}

//=============================================================================
// Agent Lifecycle with Connections Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConnectBeforeStart) {
    // Connect modules before starting agent
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionTest, ConnectAfterStart) {
    // Start agent first
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Connect modules while running
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

TEST_F(HealthAgentConnectionTest, DestroyWithConnections) {
    // Connect all modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // Destroy should handle cleanup properly
    nimcp_health_agent_destroy(agent);
    agent = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// Edge Cases Tests
//=============================================================================

TEST_F(HealthAgentConnectionTest, ConfigBoundaryValues_Prediction) {
    health_agent_prediction_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    // Test boundary values
    cfg.prediction_threshold = 0.0f;
    cfg.prediction_horizon_ms = 0;
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &cfg), 0);

    cfg.prediction_threshold = 1.0f;
    cfg.prediction_horizon_ms = UINT32_MAX;
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, &cfg), 0);
}

TEST_F(HealthAgentConnectionTest, ConfigBoundaryValues_Collective) {
    health_agent_collective_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    // Test boundary values
    cfg.consensus_threshold = 0.0f;
    cfg.consensus_timeout_ms = 0;
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &cfg), 0);

    cfg.consensus_threshold = 1.0f;
    cfg.consensus_timeout_ms = UINT32_MAX;
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, &cfg), 0);
}

TEST_F(HealthAgentConnectionTest, ConfigBoundaryValues_Gpu) {
    health_agent_gpu_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    // Test boundary values
    cfg.gpu_check_interval_ms = 0;
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, &cfg), 0);

    cfg.gpu_check_interval_ms = UINT32_MAX;
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, &cfg), 0);
}
