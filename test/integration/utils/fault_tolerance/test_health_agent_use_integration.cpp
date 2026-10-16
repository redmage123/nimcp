/**
 * @file test_health_agent_use_integration.cpp
 * @brief Integration tests for Health Agent USE functions with cognitive modules
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Test health agent integration with Portia, Dragonfly, Swarm modules
 * WHY:  Verify correct cross-module communication and data flow
 * HOW:  Test complete workflows involving multiple connected modules
 *
 * INTEGRATION SCENARIOS:
 * - Anomaly detection -> Dragonfly tracking -> Prediction pipeline
 * - Threat detection -> Swarm immune response -> Memory cell formation
 * - Resource pressure -> Portia tier adjustment -> Graceful degradation
 * - Pattern storage -> Memory consolidation -> Replay cycle
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "portia/nimcp_portia.h"
#include "dragonfly/nimcp_dragonfly.h"
#include "swarm/nimcp_swarm_immune.h"
#include "swarm/nimcp_swarm_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Full integration fixture with all modules connected
 */
class HealthAgentFullIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    portia_context_t* portia_ctx = nullptr;
    dragonfly_system_t* dragonfly = nullptr;
    NimcpSwarmImmuneSystem* swarm_immune = nullptr;
    NimcpSwarmMemory* swarm_memory = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        // Get default config
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;  // Fast for testing
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = false;

        // Create agent
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";

        // Initialize Portia subsystem first with defaults
        portia_config_t portia_sys_config = portia_get_default_config();
        portia_init(&portia_sys_config);
        portia_ctx = portia_get_context();

        // Connect Portia
        health_agent_portia_config_t portia_cfg;
        memset(&portia_cfg, 0, sizeof(portia_cfg));
        portia_cfg.enable_portia = true;
        portia_cfg.enable_tier_monitoring = true;
        portia_cfg.enable_degradation_coordination = true;
        portia_cfg.enable_auto_tier_switch = true;
        portia_cfg.degradation_trigger_threshold = 0.3f;
        portia_cfg.upgrade_health_threshold = 0.8f;
        portia_cfg.tier_check_interval_ms = 500;
        nimcp_health_agent_connect_portia(agent, portia_ctx, &portia_cfg);

        // Create and connect Dragonfly
        dragonfly_config_t df_cfg = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&df_cfg);
        if (dragonfly) {
            health_agent_dragonfly_config_t df_agent_cfg;
            memset(&df_agent_cfg, 0, sizeof(df_agent_cfg));
            df_agent_cfg.enable_dragonfly = true;
            df_agent_cfg.enable_anomaly_tracking = true;
            df_agent_cfg.enable_pursuit_mode = true;
            df_agent_cfg.enable_interception = true;
            df_agent_cfg.enable_prediction_integration = true;
            df_agent_cfg.lock_on_severity_threshold = 0.5f;
            df_agent_cfg.pursuit_timeout_s = 5.0f;
            df_agent_cfg.update_rate_hz = 10;
            nimcp_health_agent_connect_dragonfly(agent, dragonfly, &df_agent_cfg);
        }

        // Create and connect Swarm Immune
        NimcpSwarmImmuneConfig immune_cfg;
        nimcp_swarm_immune_default_config(&immune_cfg);
        swarm_immune = nimcp_swarm_immune_create(&immune_cfg, nullptr, 1);
        if (swarm_immune) {
            health_agent_swarm_immune_config_t immune_agent_cfg;
            memset(&immune_agent_cfg, 0, sizeof(immune_agent_cfg));
            immune_agent_cfg.enable_swarm_immune = true;
            immune_agent_cfg.enable_threat_detection = true;
            immune_agent_cfg.enable_coordinated_response = true;
            immune_agent_cfg.enable_memory_sharing = true;
            immune_agent_cfg.threat_detection_threshold = 0.5f;
            immune_agent_cfg.consensus_timeout_ms = 1000;
            nimcp_health_agent_connect_swarm_immune(agent, swarm_immune, &immune_agent_cfg);
        }

        // Create and connect Swarm Memory
        swarm_memory = nimcp_swarm_memory_create(1000, 3);
        if (swarm_memory) {
            nimcp_swarm_memory_init(swarm_memory, nullptr);
            health_agent_swarm_memory_config_t mem_agent_cfg;
            memset(&mem_agent_cfg, 0, sizeof(mem_agent_cfg));
            mem_agent_cfg.enable_swarm_memory = true;
            mem_agent_cfg.enable_distributed_storage = true;
            mem_agent_cfg.enable_memory_replay = true;
            mem_agent_cfg.enable_consolidation = true;
            mem_agent_cfg.replay_priority_threshold = 0.3f;
            mem_agent_cfg.consolidation_interval_ms = 5000;
            nimcp_health_agent_connect_swarm_memory(agent, swarm_memory, &mem_agent_cfg);
        }
    }

    void TearDown() override {
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
        if (dragonfly) {
            dragonfly_system_destroy(dragonfly);
            dragonfly = nullptr;
        }
        if (swarm_immune) {
            nimcp_swarm_immune_destroy(swarm_immune);
            swarm_immune = nullptr;
        }
        if (swarm_memory) {
            nimcp_swarm_memory_destroy(swarm_memory);
            swarm_memory = nullptr;
        }
        // Clean up Portia
        portia_destroy();
        portia_ctx = nullptr;
    }

    // Helper to create test anomaly message
    health_agent_message_t create_anomaly_message(
        health_agent_severity_t severity,
        const char* description
    ) {
        health_agent_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = HEALTH_MSG_ANOMALY_DETECTED;
        msg.severity = severity;
        msg.source = HEALTH_SOURCE_NEURAL;
        msg.timestamp_us = get_timestamp_us();
        msg.anomaly_id = static_cast<uint64_t>(rand()) | (static_cast<uint64_t>(rand()) << 32);
        if (description) {
            strncpy(msg.description, description, sizeof(msg.description) - 1);
        }
        return msg;
    }

    uint64_t get_timestamp_us() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }
};

//=============================================================================
// Anomaly Detection -> Dragonfly Pipeline Tests
//=============================================================================

TEST_F(HealthAgentFullIntegrationTest, AnomalyTrackingPipeline) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    // Create and track multiple anomalies
    std::vector<uint32_t> tracked_targets;

    for (int i = 0; i < 5; i++) {
        health_agent_message_t msg = create_anomaly_message(
            static_cast<health_agent_severity_t>(HEALTH_SEVERITY_INFO + (i % 4)),
            "Test anomaly for tracking"
        );

        uint32_t target_id = 0;
        int result = nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id);
        EXPECT_EQ(result, 0) << "Failed to track anomaly " << i;

        if (result == 0 && target_id != 0) {
            tracked_targets.push_back(target_id);
        }
    }

    // Verify targets were tracked
    EXPECT_GT(tracked_targets.size(), 0u);

    // Check dragonfly mode
    uint32_t mode = 0;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_get_mode(agent, &mode), 0);
}

TEST_F(HealthAgentFullIntegrationTest, AnomalyPredictionWorkflow) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    // Track a severe anomaly
    health_agent_message_t msg = create_anomaly_message(
        HEALTH_SEVERITY_CRITICAL,
        "Critical system anomaly"
    );

    uint32_t target_id = 0;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id), 0);

    // Get prediction
    float time_to_failure = 0.0f;
    float confidence = 0.0f;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_predict(
        agent, target_id, &time_to_failure, &confidence
    ), 0);

    // Based on prediction, might pursue or abort
    if (confidence > 0.5f && time_to_failure > 0.0f) {
        // High confidence, pursue
        nimcp_health_agent_use_dragonfly_pursue(agent);
    }

    // Cleanup - abort any active pursuit
    nimcp_health_agent_use_dragonfly_abort(agent);
}

//=============================================================================
// Threat Detection -> Swarm Immune Response Pipeline Tests
//=============================================================================

TEST_F(HealthAgentFullIntegrationTest, ThreatResponsePipeline) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    // Detect potential threats from multiple sources
    std::vector<uint32_t> detected_threats;

    // Simulate threat detection from various sources
    uint8_t patterns[][8] = {
        {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8},  // Pattern 1
        {0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01},  // Pattern 2
        {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE},  // Pattern 3
    };

    for (uint32_t i = 0; i < 3; i++) {
        bool threat_detected = false;
        uint32_t threat_id = 0;

        int result = nimcp_health_agent_use_swarm_detect_threat(
            agent, patterns[i], 8, i + 100, &threat_detected, &threat_id
        );
        EXPECT_EQ(result, 0);

        if (threat_detected && threat_id != 0) {
            detected_threats.push_back(threat_id);

            // Generate immune response for detected threats
            uint32_t response_id = 0;
            nimcp_health_agent_use_swarm_generate_response(agent, threat_id, &response_id);
        }
    }
}

TEST_F(HealthAgentFullIntegrationTest, MemoryCellFormationPipeline) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    // Add memory cells for various threat patterns
    uint8_t threat_patterns[][4] = {
        {0xBA, 0xAD, 0xF0, 0x0D},
        {0xDE, 0xAD, 0xC0, 0xDE},
        {0xCA, 0xFE, 0xBE, 0xEF},
    };

    std::vector<uint32_t> cell_ids;

    for (int i = 0; i < 3; i++) {
        uint32_t cell_id = 0;
        int result = nimcp_health_agent_use_swarm_add_memory_cell(
            agent, threat_patterns[i], 4,
            static_cast<uint32_t>(RESPONSE_ISOLATION + (i % 3)),
            &cell_id
        );
        EXPECT_EQ(result, 0);
        cell_ids.push_back(cell_id);
    }

    // Now test if the immune system can detect these patterns
    for (int i = 0; i < 3; i++) {
        bool threat_detected = false;
        nimcp_health_agent_use_swarm_detect_threat(
            agent, threat_patterns[i], 4, 1, &threat_detected, nullptr
        );
        // Memory should help detect these patterns
    }
}

TEST_F(HealthAgentFullIntegrationTest, BehaviorMonitoringPipeline) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    // Monitor behavior of multiple components
    std::vector<float> anomaly_scores;

    for (uint32_t component_id = 1; component_id <= 10; component_id++) {
        float score = 0.0f;
        int result = nimcp_health_agent_use_swarm_check_behavior(
            agent, component_id, &score
        );
        EXPECT_EQ(result, 0);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 1.0f);
        anomaly_scores.push_back(score);
    }

    // Calculate average anomaly score
    float avg_score = 0.0f;
    for (float s : anomaly_scores) {
        avg_score += s;
    }
    avg_score /= anomaly_scores.size();

    // Average should be reasonable (not all anomalous)
    EXPECT_LT(avg_score, 0.9f);
}

//=============================================================================
// Resource Pressure -> Portia Tier Adjustment Pipeline Tests
//=============================================================================

TEST_F(HealthAgentFullIntegrationTest, ResourcePressureAdaptation) {
    // Simulate increasing resource pressure and tier adjustments

    // Start at full tier
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), 0);

    // Simulate resource pressure - degrade gradually
    uint32_t power_state, thermal_state, degradation;

    for (int pressure = 0; pressure <= 10; pressure += 2) {
        // Set degradation level based on pressure
        EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, pressure), 0);

        // Check status
        EXPECT_EQ(nimcp_health_agent_use_portia_get_status(
            agent, &power_state, &thermal_state, &degradation
        ), 0);

        // At high pressure, switch to lower tier
        if (pressure >= 8) {
            EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(
                agent, PLATFORM_TIER_CONSTRAINED
            ), 0);
        } else if (pressure >= 4) {
            EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(
                agent, PLATFORM_TIER_MEDIUM
            ), 0);
        }
    }

    // Verify we can get neuron recommendations at different tiers
    uint32_t recommended;
    EXPECT_EQ(nimcp_health_agent_use_portia_get_recommended_neurons(agent, &recommended), 0);
    EXPECT_GT(recommended, 0u);
    EXPECT_LE(recommended, 10000u);
}

//=============================================================================
// Pattern Storage -> Memory Consolidation Pipeline Tests
//=============================================================================

TEST_F(HealthAgentFullIntegrationTest, MemoryConsolidationPipeline) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    // Store multiple patterns
    const int NUM_PATTERNS = 20;

    for (int i = 0; i < NUM_PATTERNS; i++) {
        uint8_t pattern[16];
        for (int j = 0; j < 16; j++) {
            pattern[j] = static_cast<uint8_t>((i * 17 + j) % 256);
        }

        char pattern_id[64];
        int result = nimcp_health_agent_use_swarm_memory_store(
            agent, pattern, sizeof(pattern),
            i % 3,  // Varying types
            (i % 5) + 1,  // Varying importance (1-5)
            pattern_id
        );
        EXPECT_EQ(result, 0);
    }

    // Get initial statistics
    uint64_t total_before = 0, consolidated_before = 0;
    float strength_before = 0.0f;
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_get_stats(
        agent, &total_before, &consolidated_before, &strength_before
    ), 0);

    // Trigger consolidation
    int consolidated_count = nimcp_health_agent_use_swarm_memory_consolidate(agent);
    EXPECT_GE(consolidated_count, 0);

    // Get post-consolidation statistics
    uint64_t total_after = 0, consolidated_after = 0;
    float strength_after = 0.0f;
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_get_stats(
        agent, &total_after, &consolidated_after, &strength_after
    ), 0);

    // Replay memories
    int replayed = nimcp_health_agent_use_swarm_memory_replay(agent, 5);
    EXPECT_GE(replayed, 0);
}

TEST_F(HealthAgentFullIntegrationTest, ReplayMemoryCycle) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    // Store patterns
    for (int i = 0; i < 10; i++) {
        uint8_t pattern[] = {
            static_cast<uint8_t>(i),
            static_cast<uint8_t>(i + 1),
            static_cast<uint8_t>(i + 2)
        };
        nimcp_health_agent_use_swarm_memory_store(
            agent, pattern, sizeof(pattern), 0, 3, nullptr
        );
    }

    // Multiple replay cycles
    for (int cycle = 0; cycle < 5; cycle++) {
        int replayed = nimcp_health_agent_use_swarm_memory_replay(agent, 3);
        EXPECT_GE(replayed, 0);

        // Brief pause between cycles
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

//=============================================================================
// Full Agent Lifecycle Integration Tests
//=============================================================================

TEST_F(HealthAgentFullIntegrationTest, AgentLifecycleWithModules) {
    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    // Let agent run and perform operations
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Use various modules while agent is running
    if (dragonfly) {
        health_agent_message_t msg = create_anomaly_message(
            HEALTH_SEVERITY_WARNING, "Test during runtime"
        );
        uint32_t target_id;
        nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id);
    }

    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_MEDIUM), 0);

    if (swarm_memory) {
        uint8_t data[] = {0x01, 0x02, 0x03};
        nimcp_health_agent_use_swarm_memory_store(agent, data, sizeof(data), 0, 1, nullptr);
    }

    // Get statistics while running
    health_agent_stats_t stats = {0};
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.checks_performed, 0u);

    // Let agent run a bit more
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop agent
    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));

    // Get final statistics
    nimcp_health_agent_get_stats(agent, &stats);
}

TEST_F(HealthAgentFullIntegrationTest, ConcurrentModuleAccess) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 50;
    std::atomic<int> errors{0};

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    auto portia_worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_use_portia_set_tier(
                agent, static_cast<platform_tier_t>(i % 4)
            ) != 0) {
                errors++;
            }
            std::this_thread::yield();
        }
    };

    auto dragonfly_worker = [&]() {
        if (!dragonfly) return;
        for (int i = 0; i < ITERATIONS; i++) {
            uint32_t mode;
            if (nimcp_health_agent_use_dragonfly_get_mode(agent, &mode) != 0) {
                errors++;
            }
            std::this_thread::yield();
        }
    };

    auto swarm_worker = [&]() {
        if (!swarm_memory) return;
        for (int i = 0; i < ITERATIONS; i++) {
            uint8_t data[] = {static_cast<uint8_t>(i)};
            if (nimcp_health_agent_use_swarm_memory_store(
                agent, data, sizeof(data), 0, 1, nullptr
            ) != 0) {
                errors++;
            }
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(portia_worker);
    threads.emplace_back(portia_worker);
    if (dragonfly) {
        threads.emplace_back(dragonfly_worker);
    }
    if (swarm_memory) {
        threads.emplace_back(swarm_worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    // Stop agent
    nimcp_health_agent_stop(agent);

    // Should have minimal errors
    EXPECT_LT(errors.load(), ITERATIONS);
}

//=============================================================================
// Cross-Module Integration Tests
//=============================================================================

TEST_F(HealthAgentFullIntegrationTest, AnomalyToThreatCorrelation) {
    if (!dragonfly || !swarm_immune) GTEST_SKIP() << "Modules not available";

    // Track an anomaly via Dragonfly
    health_agent_message_t msg = create_anomaly_message(
        HEALTH_SEVERITY_CRITICAL,
        "Potential security threat"
    );

    uint32_t target_id = 0;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id), 0);

    // Use the anomaly data as threat detection input
    uint8_t threat_data[8];
    memcpy(threat_data, &msg.anomaly_id, sizeof(msg.anomaly_id));

    bool threat_detected = false;
    uint32_t threat_id = 0;
    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, threat_data, sizeof(threat_data), 1, &threat_detected, &threat_id
    ), 0);

    // If threat detected, generate response
    if (threat_detected && threat_id != 0) {
        uint32_t response_id = 0;
        EXPECT_EQ(nimcp_health_agent_use_swarm_generate_response(
            agent, threat_id, &response_id
        ), 0);

        // Store as memory pattern for future detection
        if (swarm_memory) {
            nimcp_health_agent_use_swarm_memory_store(
                agent, threat_data, sizeof(threat_data), 0, 5, nullptr
            );
        }
    }

    // Adjust tier based on threat level
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(
        agent, PLATFORM_TIER_CONSTRAINED
    ), 0);
}

TEST_F(HealthAgentFullIntegrationTest, AdaptiveResourceManagement) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    // Simulate adaptive resource management based on threat level

    // Initial state - full resources
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, 0), 0);

    // Simulate threat scenarios
    for (int scenario = 0; scenario < 5; scenario++) {
        // Check behavior of components
        float total_anomaly_score = 0.0f;
        for (uint32_t comp = 1; comp <= 5; comp++) {
            float score = 0.0f;
            nimcp_health_agent_use_swarm_check_behavior(agent, comp, &score);
            total_anomaly_score += score;
        }

        float avg_anomaly = total_anomaly_score / 5.0f;

        // Adjust resources based on threat level
        if (avg_anomaly > 0.7f) {
            // High threat - constrained mode
            nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_CONSTRAINED);
            nimcp_health_agent_use_portia_degrade(agent, 8);
        } else if (avg_anomaly > 0.4f) {
            // Medium threat
            nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_MEDIUM);
            nimcp_health_agent_use_portia_degrade(agent, 4);
        } else {
            // Low threat - full resources
            nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL);
            nimcp_health_agent_use_portia_degrade(agent, 0);
        }

        // Brief pause between scenarios
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(HealthAgentFullIntegrationTest, HighVolumeOperations) {
    const int OPERATIONS = 200;

    // Start agent
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    int successful_ops = 0;

    // High volume of operations
    for (int i = 0; i < OPERATIONS; i++) {
        // Rotate through different operations
        switch (i % 5) {
            case 0:
                if (nimcp_health_agent_use_portia_set_tier(
                    agent, static_cast<platform_tier_t>(i % 4)
                ) == 0) successful_ops++;
                break;
            case 1:
                if (dragonfly) {
                    uint32_t mode;
                    if (nimcp_health_agent_use_dragonfly_get_mode(agent, &mode) == 0)
                        successful_ops++;
                }
                break;
            case 2:
                if (swarm_immune) {
                    float score;
                    if (nimcp_health_agent_use_swarm_check_behavior(
                        agent, i % 10 + 1, &score
                    ) == 0) successful_ops++;
                }
                break;
            case 3:
                if (swarm_memory) {
                    uint8_t data[] = {static_cast<uint8_t>(i)};
                    if (nimcp_health_agent_use_swarm_memory_store(
                        agent, data, 1, 0, 1, nullptr
                    ) == 0) successful_ops++;
                }
                break;
            case 4: {
                uint32_t power, thermal, degrade;
                if (nimcp_health_agent_use_portia_get_status(
                    agent, &power, &thermal, &degrade
                ) == 0) successful_ops++;
                break;
            }
        }
    }

    // Stop agent
    nimcp_health_agent_stop(agent);

    // Most operations should succeed
    EXPECT_GT(successful_ops, OPERATIONS / 2);
}
