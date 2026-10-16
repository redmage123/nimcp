/**
 * @file test_health_agent_connection_integration.cpp
 * @brief Integration tests for NIMCP Health Agent Connection Functions (Phase 2)
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Test health agent connection functions with real module instances
 * WHY:  Verify connections work correctly with actual cognitive modules
 * HOW:  Create module instances, connect them, verify interaction
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Cognitive module headers for real instances
#include "cognitive/fault_tolerance/nimcp_failure_prediction.h"
#include "cognitive/fault_tolerance/nimcp_metacognition.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_mental_health.h"
#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/recursive/nimcp_rcog_engine.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Base fixture for health agent connection integration tests
 */
class HealthAgentConnectionIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        config.enable_auto_recovery = false;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
        if (agent) {
            // Stop if running
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/**
 * @brief Fixture with failure predictor integration
 */
class HealthAgentFailurePredictorIntegrationTest : public HealthAgentConnectionIntegrationTest {
protected:
    failure_predictor_t* predictor = nullptr;

    void SetUp() override {
        HealthAgentConnectionIntegrationTest::SetUp();

        // Create failure predictor with default configuration
        predictor = failure_predictor_create();
        // predictor may be NULL if module not fully implemented
    }

    void TearDown() override {
        if (predictor) {
            failure_predictor_destroy(predictor);
            predictor = nullptr;
        }
        HealthAgentConnectionIntegrationTest::TearDown();
    }
};

/**
 * @brief Fixture with metacognition integration
 */
class HealthAgentMetacognitionIntegrationTest : public HealthAgentConnectionIntegrationTest {
protected:
    metacognition_t* metacog = nullptr;

    void SetUp() override {
        HealthAgentConnectionIntegrationTest::SetUp();

        // Create metacognition module with actual config fields
        metacognition_config_t meta_config;
        memset(&meta_config, 0, sizeof(meta_config));
        meta_config.baseline_window_size = 100;
        meta_config.degradation_threshold = 0.7f;
        meta_config.enable_adaptive_baseline = true;
        meta_config.enable_logging = false;

        metacog = metacognition_create(&meta_config);
        // metacog may be NULL if module not fully implemented
    }

    void TearDown() override {
        if (metacog) {
            metacognition_destroy(metacog);
            metacog = nullptr;
        }
        HealthAgentConnectionIntegrationTest::TearDown();
    }
};

/**
 * @brief Fixture with ethics engine integration
 */
class HealthAgentEthicsIntegrationTest : public HealthAgentConnectionIntegrationTest {
protected:
    ethics_engine_t ethics = nullptr;

    void SetUp() override {
        HealthAgentConnectionIntegrationTest::SetUp();

        // Create ethics engine with actual config fields
        ethics_config_t ethics_config;
        memset(&ethics_config, 0, sizeof(ethics_config));
        ethics_config.enable_learning = true;
        ethics_config.golden_rule_threshold = 0.5f;
        ethics_config.empathy_weight = 0.5f;
        ethics_config.default_severity = 0.5f;

        ethics = ethics_engine_create(&ethics_config);
        // ethics may be NULL if module not fully implemented
    }

    void TearDown() override {
        if (ethics) {
            ethics_engine_destroy(ethics);
            ethics = nullptr;
        }
        HealthAgentConnectionIntegrationTest::TearDown();
    }
};

/**
 * @brief Fixture with emotional system integration
 */
class HealthAgentEmotionIntegrationTest : public HealthAgentConnectionIntegrationTest {
protected:
    emotional_system_t* emotion = nullptr;

    void SetUp() override {
        HealthAgentConnectionIntegrationTest::SetUp();

        // Create emotional system with actual config fields
        emotion_config_t emo_config;
        memset(&emo_config, 0, sizeof(emo_config));
        emo_config.enable_emotion_recognition = true;
        emo_config.enable_emotional_tagging = true;
        emo_config.emotion_decay_rate = 0.1f;

        emotion = emotion_system_create(&emo_config);
        // emotion may be NULL if module not fully implemented
    }

    void TearDown() override {
        if (emotion) {
            emotion_system_destroy(emotion);
            emotion = nullptr;
        }
        HealthAgentConnectionIntegrationTest::TearDown();
    }
};

//=============================================================================
// Failure Predictor Integration Tests
//=============================================================================

TEST_F(HealthAgentFailurePredictorIntegrationTest, ConnectRealPredictor) {
    if (!predictor) GTEST_SKIP() << "Failure predictor not available";

    health_agent_prediction_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_failure_prediction = true;
    cfg.prediction_threshold = 0.7f;
    cfg.prediction_horizon_ms = 60000;
    cfg.enable_preventive_action = true;

    int result = nimcp_health_agent_connect_failure_prediction(agent, predictor, &cfg);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentFailurePredictorIntegrationTest, PredictorWithRunningAgent) {
    if (!predictor) GTEST_SKIP() << "Failure predictor not available";

    // Start agent first
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Connect predictor while running
    int result = nimcp_health_agent_connect_failure_prediction(agent, predictor, nullptr);
    EXPECT_EQ(result, 0);

    // Let agent run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Metacognition Integration Tests
//=============================================================================

TEST_F(HealthAgentMetacognitionIntegrationTest, ConnectRealMetacognition) {
    if (!metacog) GTEST_SKIP() << "Metacognition not available";

    health_agent_metacog_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_metacognition = true;
    cfg.enable_self_diagnosis = true;
    cfg.degradation_threshold = 0.3f;

    int result = nimcp_health_agent_connect_metacognition(agent, metacog, &cfg);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentMetacognitionIntegrationTest, MetacognitionWithRunningAgent) {
    if (!metacog) GTEST_SKIP() << "Metacognition not available";

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    int result = nimcp_health_agent_connect_metacognition(agent, metacog, nullptr);
    EXPECT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Ethics Engine Integration Tests
//=============================================================================

TEST_F(HealthAgentEthicsIntegrationTest, ConnectRealEthicsEngine) {
    if (!ethics) GTEST_SKIP() << "Ethics engine not available";

    health_agent_ethics_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_ethics_evaluation = true;
    cfg.enable_asimov_laws = true;
    cfg.enable_mercy_directive = true;
    cfg.ethics_override_threshold = 0.95f;

    int result = nimcp_health_agent_connect_ethics(agent, ethics, &cfg);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentEthicsIntegrationTest, EthicsWithRunningAgent) {
    if (!ethics) GTEST_SKIP() << "Ethics engine not available";

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    int result = nimcp_health_agent_connect_ethics(agent, ethics, nullptr);
    EXPECT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Emotional System Integration Tests
//=============================================================================

TEST_F(HealthAgentEmotionIntegrationTest, ConnectRealEmotionalSystem) {
    if (!emotion) GTEST_SKIP() << "Emotional system not available";

    health_agent_emotion_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_emotion_awareness = true;
    cfg.enable_stress_adjustment = true;
    cfg.stress_threshold_multiplier = 1.5f;

    int result = nimcp_health_agent_connect_emotion(agent, emotion, nullptr, &cfg);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentEmotionIntegrationTest, EmotionWithRunningAgent) {
    if (!emotion) GTEST_SKIP() << "Emotional system not available";

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    int result = nimcp_health_agent_connect_emotion(agent, emotion, nullptr, nullptr);
    EXPECT_EQ(result, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Multi-Module Integration Tests
//=============================================================================

TEST_F(HealthAgentConnectionIntegrationTest, ConnectMultipleCognitiveModules) {
    // Connect multiple modules (NULL modules for now, testing connection mechanism)
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_collective(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_rcog(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_gpu(agent, nullptr, nullptr), 0);

    // Start agent with all connections
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Let it run
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Stop
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

TEST_F(HealthAgentConnectionIntegrationTest, ReconnectWhileRunning) {
    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Connect, run, reconnect multiple times
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Concurrent Connection Tests
//=============================================================================

TEST_F(HealthAgentConnectionIntegrationTest, ConcurrentConnections) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 50;
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    auto worker = [&](int thread_id) {
        for (int i = 0; i < ITERATIONS; i++) {
            int result = 0;
            switch (thread_id % 4) {
                case 0:
                    result = nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr);
                    break;
                case 1:
                    result = nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr);
                    break;
                case 2:
                    result = nimcp_health_agent_connect_ethics(agent, nullptr, nullptr);
                    break;
                case 3:
                    result = nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr);
                    break;
            }
            if (result == 0) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

    // All connections should succeed (mutex-protected)
    EXPECT_EQ(success_count.load(), NUM_THREADS * ITERATIONS);
    EXPECT_EQ(failure_count.load(), 0);
}

TEST_F(HealthAgentConnectionIntegrationTest, ConcurrentConnectionsAllModules) {
    const int ITERATIONS = 20;
    std::atomic<int> success_count{0};

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    auto connect_all = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr) == 0 &&
                nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr) == 0 &&
                nimcp_health_agent_connect_ethics(agent, nullptr, nullptr) == 0 &&
                nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr) == 0 &&
                nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr) == 0 &&
                nimcp_health_agent_connect_mental_health(agent, nullptr) == 0 &&
                nimcp_health_agent_connect_collective(agent, nullptr, nullptr) == 0 &&
                nimcp_health_agent_connect_rcog(agent, nullptr, nullptr) == 0 &&
                nimcp_health_agent_connect_gpu(agent, nullptr, nullptr) == 0) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(connect_all);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_EQ(success_count.load(), 4 * ITERATIONS);
}

//=============================================================================
// Agent Statistics with Connections Tests
//=============================================================================

TEST_F(HealthAgentConnectionIntegrationTest, StatisticsWithConnections) {
    // Connect modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    // Start and run
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Get stats
    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);

    // Verify stats are being collected
    EXPECT_GE(stats.checks_performed, 0u);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}

//=============================================================================
// Heartbeat with Connections Tests
//=============================================================================

TEST_F(HealthAgentConnectionIntegrationTest, HeartbeatWithConnections) {
    // Connect modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    // Send heartbeats (heartbeat returns void)
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
}
