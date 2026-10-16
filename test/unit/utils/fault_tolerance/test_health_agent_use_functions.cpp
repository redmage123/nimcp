/**
 * @file test_health_agent_use_functions.cpp
 * @brief Unit tests for NIMCP Health Agent USE functions
 * @version 1.0.0
 * @date 2025-01-17
 *
 * WHAT: Test health agent USE functions for Portia, Dragonfly, Swarm modules
 * WHY:  Ensure USE functions correctly integrate with connected cognitive modules
 * HOW:  Test each USE function with valid inputs, edge cases, and error conditions
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <vector>

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
 * @brief Base fixture for health agent tests
 */
class HealthAgentUSETest : public ::testing::Test {
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

    health_agent_message_t create_test_message(
        health_agent_msg_type_t type,
        health_agent_severity_t severity
    ) {
        health_agent_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = type;
        msg.severity = severity;
        msg.source = HEALTH_SOURCE_NEURAL;
        msg.timestamp_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        msg.anomaly_id = static_cast<uint64_t>(rand());
        snprintf(msg.description, sizeof(msg.description), "Test anomaly");
        return msg;
    }
};

/**
 * @brief Fixture with Portia module connected
 */
class HealthAgentPortiaTest : public HealthAgentUSETest {
protected:
    portia_context_t* portia_ctx = nullptr;

    void SetUp() override {
        HealthAgentUSETest::SetUp();

        // Initialize Portia subsystem first with defaults
        portia_config_t portia_sys_config = portia_get_default_config();
        portia_init(&portia_sys_config);
        portia_ctx = portia_get_context();

        health_agent_portia_config_t portia_config;
        memset(&portia_config, 0, sizeof(portia_config));
        portia_config.enable_portia = true;
        portia_config.enable_tier_monitoring = true;
        portia_config.enable_power_awareness = true;
        portia_config.enable_thermal_monitoring = true;
        portia_config.enable_degradation_coordination = true;
        portia_config.enable_auto_tier_switch = true;
        portia_config.degradation_trigger_threshold = 0.3f;
        portia_config.upgrade_health_threshold = 0.8f;
        portia_config.tier_check_interval_ms = 1000;

        int result = nimcp_health_agent_connect_portia(agent, portia_ctx, &portia_config);
        ASSERT_EQ(result, 0) << "Failed to connect Portia module";
    }

    void TearDown() override {
        HealthAgentUSETest::TearDown();
        // Portia cleanup
        portia_destroy();
        portia_ctx = nullptr;
    }
};

/**
 * @brief Fixture with Dragonfly module connected
 */
class HealthAgentDragonflyTest : public HealthAgentUSETest {
protected:
    dragonfly_system_t* dragonfly = nullptr;

    void SetUp() override {
        HealthAgentUSETest::SetUp();

        dragonfly_config_t df_config = dragonfly_default_config();
        dragonfly = dragonfly_system_create(&df_config);

        if (dragonfly) {
            health_agent_dragonfly_config_t agent_df_config = {0};
            agent_df_config.enable_dragonfly = true;
            agent_df_config.enable_anomaly_tracking = true;
            agent_df_config.enable_pursuit_mode = true;
            agent_df_config.enable_interception = true;
            agent_df_config.enable_prediction_integration = true;
            agent_df_config.lock_on_severity_threshold = 0.5f;
            agent_df_config.pursuit_timeout_s = 5.0f;
            agent_df_config.update_rate_hz = 10;

            int result = nimcp_health_agent_connect_dragonfly(
                agent, dragonfly, &agent_df_config
            );
            ASSERT_EQ(result, 0) << "Failed to connect Dragonfly module";
        }
    }

    void TearDown() override {
        HealthAgentUSETest::TearDown();
        if (dragonfly) {
            dragonfly_system_destroy(dragonfly);
            dragonfly = nullptr;
        }
    }
};

/**
 * @brief Fixture with Swarm Immune module connected
 */
class HealthAgentSwarmImmuneTest : public HealthAgentUSETest {
protected:
    NimcpSwarmImmuneSystem* swarm_immune = nullptr;

    void SetUp() override {
        HealthAgentUSETest::SetUp();

        NimcpSwarmImmuneConfig immune_config;
        nimcp_swarm_immune_default_config(&immune_config);
        swarm_immune = nimcp_swarm_immune_create(&immune_config, nullptr, 1);

        if (swarm_immune) {
            health_agent_swarm_immune_config_t agent_config = {0};
            agent_config.enable_swarm_immune = true;
            agent_config.enable_threat_detection = true;
            agent_config.enable_coordinated_response = true;
            agent_config.enable_memory_sharing = true;
            agent_config.enable_self_verification = true;
            agent_config.threat_detection_threshold = 0.5f;
            agent_config.consensus_timeout_ms = 1000;

            int result = nimcp_health_agent_connect_swarm_immune(
                agent, swarm_immune, &agent_config
            );
            ASSERT_EQ(result, 0) << "Failed to connect Swarm Immune module";
        }
    }

    void TearDown() override {
        HealthAgentUSETest::TearDown();
        if (swarm_immune) {
            nimcp_swarm_immune_destroy(swarm_immune);
            swarm_immune = nullptr;
        }
    }
};

/**
 * @brief Fixture with Swarm Memory module connected
 */
class HealthAgentSwarmMemoryTest : public HealthAgentUSETest {
protected:
    NimcpSwarmMemory* swarm_memory = nullptr;

    void SetUp() override {
        HealthAgentUSETest::SetUp();

        // Create swarm memory with capacity and replication factor
        swarm_memory = nimcp_swarm_memory_create(1000, 3);

        if (swarm_memory) {
            // Initialize the swarm memory system
            nimcp_swarm_memory_init(swarm_memory, nullptr);

            health_agent_swarm_memory_config_t agent_config;
            memset(&agent_config, 0, sizeof(agent_config));
            agent_config.enable_swarm_memory = true;
            agent_config.enable_distributed_storage = true;
            agent_config.enable_memory_replay = true;
            agent_config.enable_consolidation = true;
            agent_config.enable_forgetting = false;
            agent_config.replay_priority_threshold = 0.3f;
            agent_config.consolidation_interval_ms = 5000;

            int result = nimcp_health_agent_connect_swarm_memory(
                agent, swarm_memory, &agent_config
            );
            ASSERT_EQ(result, 0) << "Failed to connect Swarm Memory module";
        }
    }

    void TearDown() override {
        HealthAgentUSETest::TearDown();
        if (swarm_memory) {
            nimcp_swarm_memory_destroy(swarm_memory);
            swarm_memory = nullptr;
        }
    }
};

//=============================================================================
// Portia USE Function Tests
//=============================================================================

TEST_F(HealthAgentPortiaTest, SetTierValid) {
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_MEDIUM), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_CONSTRAINED), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_MINIMAL), 0);
}

TEST_F(HealthAgentPortiaTest, SetTierNullAgent) {
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(nullptr, PLATFORM_TIER_FULL), -1);
}

TEST_F(HealthAgentPortiaTest, GetStatusValid) {
    uint32_t power_state = 0;
    uint32_t thermal_state = 0;
    uint32_t degradation = 0;

    int result = nimcp_health_agent_use_portia_get_status(
        agent, &power_state, &thermal_state, &degradation
    );
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentPortiaTest, GetStatusNullOutputs) {
    int result = nimcp_health_agent_use_portia_get_status(agent, nullptr, nullptr, nullptr);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentPortiaTest, GetStatusNullAgent) {
    uint32_t power_state = 0;
    EXPECT_EQ(nimcp_health_agent_use_portia_get_status(nullptr, &power_state, nullptr, nullptr), -1);
}

TEST_F(HealthAgentPortiaTest, DegradeLevel) {
    EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, 0), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, 2), 0);
    EXPECT_EQ(nimcp_health_agent_use_portia_degrade(agent, 4), 0);
}

TEST_F(HealthAgentPortiaTest, GetRecommendedNeurons) {
    uint32_t recommended = 0;
    int result = nimcp_health_agent_use_portia_get_recommended_neurons(agent, &recommended);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentPortiaTest, GetRecommendedNeuronsNullOutput) {
    EXPECT_EQ(nimcp_health_agent_use_portia_get_recommended_neurons(agent, nullptr), -1);
}

TEST_F(HealthAgentUSETest, PortiaNotConnected) {
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_FULL), -1);

    uint32_t power_state;
    EXPECT_EQ(nimcp_health_agent_use_portia_get_status(agent, &power_state, nullptr, nullptr), -1);
}

//=============================================================================
// Dragonfly USE Function Tests
//=============================================================================

TEST_F(HealthAgentDragonflyTest, TrackAnomalyValid) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    health_agent_message_t msg = create_test_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING
    );

    uint32_t target_id = 0;
    int result = nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentDragonflyTest, TrackAnomalyNullMessage) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    uint32_t target_id = 0;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, nullptr, &target_id), -1);
}

TEST_F(HealthAgentDragonflyTest, TrackAnomalyNullTargetId) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    health_agent_message_t msg = create_test_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING
    );

    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, nullptr), -1);
}

TEST_F(HealthAgentDragonflyTest, PredictNoTarget) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    float time_to_failure = 0.0f;
    float confidence = 0.0f;

    int result = nimcp_health_agent_use_dragonfly_predict(
        agent, 0, &time_to_failure, &confidence
    );
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentDragonflyTest, PursueAndAbort) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    // Abort should succeed even without active pursuit
    int abort_result = nimcp_health_agent_use_dragonfly_abort(agent);
    EXPECT_EQ(abort_result, 0);
}

TEST_F(HealthAgentDragonflyTest, GetMode) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    uint32_t mode = 0;
    int result = nimcp_health_agent_use_dragonfly_get_mode(agent, &mode);
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentDragonflyTest, GetModeNullOutput) {
    if (!dragonfly) GTEST_SKIP() << "Dragonfly not available";

    EXPECT_EQ(nimcp_health_agent_use_dragonfly_get_mode(agent, nullptr), -1);
}

TEST_F(HealthAgentUSETest, DragonflyNotConnected) {
    health_agent_message_t msg = create_test_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING
    );

    uint32_t target_id = 0;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_track_anomaly(agent, &msg, &target_id), -1);

    uint32_t mode = 0;
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_get_mode(agent, &mode), -1);
}

//=============================================================================
// Swarm Immune USE Function Tests
//=============================================================================

TEST_F(HealthAgentSwarmImmuneTest, DetectThreat) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    bool threat_detected = false;
    uint32_t threat_id = 0;

    int result = nimcp_health_agent_use_swarm_detect_threat(
        agent, data, sizeof(data), 1, &threat_detected, &threat_id
    );
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentSwarmImmuneTest, DetectThreatNullData) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    bool threat_detected = false;
    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, nullptr, 0, 1, &threat_detected, nullptr
    ), -1);
}

TEST_F(HealthAgentSwarmImmuneTest, DetectThreatNullOutput) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    uint8_t data[] = {0x01, 0x02};
    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, data, sizeof(data), 1, nullptr, nullptr
    ), -1);
}

TEST_F(HealthAgentSwarmImmuneTest, CheckBehavior) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    float anomaly_score = 0.0f;
    int result = nimcp_health_agent_use_swarm_check_behavior(
        agent, 1, &anomaly_score
    );
    EXPECT_EQ(result, 0);
    EXPECT_GE(anomaly_score, 0.0f);
    EXPECT_LE(anomaly_score, 1.0f);
}

TEST_F(HealthAgentSwarmImmuneTest, CheckBehaviorNullOutput) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    EXPECT_EQ(nimcp_health_agent_use_swarm_check_behavior(agent, 1, nullptr), -1);
}

TEST_F(HealthAgentSwarmImmuneTest, AddMemoryCell) {
    if (!swarm_immune) GTEST_SKIP() << "Swarm Immune not available";

    uint8_t pattern[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint32_t cell_id = 0;

    int result = nimcp_health_agent_use_swarm_add_memory_cell(
        agent, pattern, sizeof(pattern),
        static_cast<uint32_t>(RESPONSE_ISOLATION), &cell_id
    );
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentUSETest, SwarmImmuneNotConnected) {
    uint8_t data[] = {0x01, 0x02};
    bool threat_detected = false;

    EXPECT_EQ(nimcp_health_agent_use_swarm_detect_threat(
        agent, data, sizeof(data), 1, &threat_detected, nullptr
    ), -1);

    float score = 0.0f;
    EXPECT_EQ(nimcp_health_agent_use_swarm_check_behavior(agent, 1, &score), -1);
}

//=============================================================================
// Swarm Memory USE Function Tests
//=============================================================================

TEST_F(HealthAgentSwarmMemoryTest, StorePattern) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    uint8_t pattern_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    char pattern_id[64] = {0};

    int result = nimcp_health_agent_use_swarm_memory_store(
        agent, pattern_data, sizeof(pattern_data),
        0,  // pattern_type
        1,  // importance
        pattern_id
    );
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentSwarmMemoryTest, StorePatternNullData) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    char pattern_id[64] = {0};
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_store(
        agent, nullptr, 0, 0, 1, pattern_id
    ), -1);
}

TEST_F(HealthAgentSwarmMemoryTest, ReplayCycle) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    // Store some patterns first
    for (int i = 0; i < 5; i++) {
        uint8_t pattern[] = {static_cast<uint8_t>(i), static_cast<uint8_t>(i+1)};
        nimcp_health_agent_use_swarm_memory_store(
            agent, pattern, sizeof(pattern), 0, 1, nullptr
        );
    }

    int replayed = nimcp_health_agent_use_swarm_memory_replay(agent, 3);
    EXPECT_GE(replayed, 0);
}

TEST_F(HealthAgentSwarmMemoryTest, Consolidate) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    for (int i = 0; i < 10; i++) {
        uint8_t pattern[] = {static_cast<uint8_t>(i)};
        nimcp_health_agent_use_swarm_memory_store(
            agent, pattern, sizeof(pattern), 0, 1, nullptr
        );
    }

    int consolidated = nimcp_health_agent_use_swarm_memory_consolidate(agent);
    EXPECT_GE(consolidated, 0);
}

TEST_F(HealthAgentSwarmMemoryTest, GetStatistics) {
    if (!swarm_memory) GTEST_SKIP() << "Swarm Memory not available";

    uint64_t total_memories = 0;
    uint64_t consolidated = 0;
    float avg_strength = 0.0f;

    int result = nimcp_health_agent_use_swarm_memory_get_stats(
        agent, &total_memories, &consolidated, &avg_strength
    );
    EXPECT_EQ(result, 0);
}

TEST_F(HealthAgentUSETest, SwarmMemoryNotConnected) {
    uint8_t data[] = {0x01};
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_store(
        agent, data, sizeof(data), 0, 1, nullptr
    ), -1);

    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_replay(agent, 5), -1);
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_consolidate(agent), -1);
}

//=============================================================================
// Engram USE Function Tests
//=============================================================================

TEST_F(HealthAgentUSETest, EngramEncodeNotConnected) {
    health_agent_message_t msg = create_test_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING
    );

    uint64_t engram_id = 0;
    EXPECT_EQ(nimcp_health_agent_use_engram_encode(agent, &msg, &engram_id), -1);
}

TEST_F(HealthAgentUSETest, EngramEncodeNullMessage) {
    uint64_t engram_id = 0;
    EXPECT_EQ(nimcp_health_agent_use_engram_encode(agent, nullptr, &engram_id), -1);
}

TEST_F(HealthAgentUSETest, EngramRecallNotConnected) {
    health_agent_message_t msg = create_test_message(
        HEALTH_MSG_ANOMALY_DETECTED,
        HEALTH_SEVERITY_WARNING
    );
    uint64_t recalled_ids[10] = {0};
    uint32_t num_recalled = 0;
    EXPECT_EQ(nimcp_health_agent_use_engram_recall(
        agent, &msg, recalled_ids, 10, &num_recalled
    ), -1);
}

TEST_F(HealthAgentUSETest, EngramGetStatsNotConnected) {
    uint32_t active = 0;
    uint32_t consolidated = 0;
    float avg_strength = 0.0f;
    EXPECT_EQ(nimcp_health_agent_use_engram_get_stats(
        agent, &active, &consolidated, &avg_strength
    ), -1);
}

//=============================================================================
// Systems Consolidation USE Function Tests
//=============================================================================

TEST_F(HealthAgentUSETest, ConsolidationReplayNotConnected) {
    EXPECT_EQ(nimcp_health_agent_use_consolidation_replay(agent, 5), -1);
}

TEST_F(HealthAgentUSETest, ConsolidationExtractSemanticsNotConnected) {
    EXPECT_EQ(nimcp_health_agent_use_consolidation_extract_semantics(agent), -1);
}

TEST_F(HealthAgentUSETest, ConsolidationGetStatsNotConnected) {
    uint32_t cortical_nodes = 0;
    uint64_t total_replays = 0;
    uint64_t total_transfers = 0;
    EXPECT_EQ(nimcp_health_agent_use_consolidation_get_stats(
        agent, &cortical_nodes, &total_replays, &total_transfers
    ), -1);
}

//=============================================================================
// Agent Lifecycle with USE Functions
//=============================================================================

TEST_F(HealthAgentUSETest, CreateDestroyWithConfig) {
    health_agent_config_t custom_config;
    nimcp_health_agent_default_config(&custom_config);
    custom_config.check_interval_ms = 50;
    custom_config.watchdog_timeout_ms = 10000;

    nimcp_health_agent_t* custom_agent = nimcp_health_agent_create(&custom_config);
    ASSERT_NE(custom_agent, nullptr);

    nimcp_health_agent_destroy(custom_agent);
}

TEST_F(HealthAgentUSETest, StartStopAgent) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    EXPECT_TRUE(nimcp_health_agent_is_running(agent));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    EXPECT_FALSE(nimcp_health_agent_is_running(agent));
}

TEST_F(HealthAgentUSETest, GetStatistics) {
    health_agent_stats_t stats = {0};
    nimcp_health_agent_get_stats(agent, &stats);
    EXPECT_GE(stats.checks_performed, 0u);
}

//=============================================================================
// Concurrent Access Tests
//=============================================================================

TEST_F(HealthAgentPortiaTest, ConcurrentPortiaAccess) {
    const int NUM_THREADS = 4;
    const int ITERATIONS = 100;
    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < ITERATIONS; i++) {
            if (nimcp_health_agent_use_portia_set_tier(
                agent, static_cast<uint32_t>(i % 4)
            ) == 0) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count.load(), 0);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(HealthAgentUSETest, NullAgentAllFunctions) {
    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(nullptr, PLATFORM_TIER_FULL), -1);
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_pursue(nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_use_dragonfly_abort(nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_use_swarm_memory_consolidate(nullptr), -1);
    EXPECT_EQ(nimcp_health_agent_use_consolidation_replay(nullptr, 5), -1);
    EXPECT_EQ(nimcp_health_agent_use_consolidation_extract_semantics(nullptr), -1);
}

TEST_F(HealthAgentUSETest, MultipleModuleConnect) {
    // Initialize Portia subsystem first with defaults
    portia_config_t portia_sys_config = portia_get_default_config();
    portia_init(&portia_sys_config);
    portia_context_t* portia_ctx = portia_get_context();

    health_agent_portia_config_t portia_cfg;
    memset(&portia_cfg, 0, sizeof(portia_cfg));
    portia_cfg.enable_portia = true;
    portia_cfg.enable_tier_monitoring = true;
    portia_cfg.enable_auto_tier_switch = true;
    EXPECT_EQ(nimcp_health_agent_connect_portia(agent, portia_ctx, &portia_cfg), 0);

    EXPECT_EQ(nimcp_health_agent_use_portia_set_tier(agent, PLATFORM_TIER_MEDIUM), 0);

    // Cleanup
    portia_destroy();
}
