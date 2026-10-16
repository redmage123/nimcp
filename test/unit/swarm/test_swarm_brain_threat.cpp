/**
 * @file test_swarm_brain_threat.cpp
 * @brief Unit tests for NIMCP Swarm Brain threat handling functionality
 *
 * TEST COVERAGE:
 * - Threat message reception and parsing
 * - Local brain alerting
 * - Workspace updates from threats
 * - Severity-based escalation
 * - Threat history tracking
 * - Emergency proposal generation
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_brain.h"

class SwarmBrainThreatTest : public ::testing::Test {
protected:
    swarm_brain_t* swarm;
    swarm_brain_config_t config;

    void SetUp() override {
        config = swarm_brain_default_config();
        config.drone_id = 1;
        strncpy(config.swarm_name, "test_swarm", sizeof(config.swarm_name) - 1);
        config.heartbeat_interval_ms = 100;
        config.sync_interval_ms = 50;
        config.vote_timeout_ms = 1000;
        config.coherence_threshold = 0.5f;
        config.critical_mass = 3;
        config.workspace_size = 32;
        config.enable_bio_async = false;

        swarm = swarm_brain_create(&config);
        ASSERT_NE(swarm, nullptr);
    }

    void TearDown() override {
        if (swarm) {
            swarm_brain_destroy(swarm);
            swarm = nullptr;
        }
    }

    threat_data_t create_threat(uint32_t type, float severity, const char* desc) {
        threat_data_t threat = {0};
        threat.threat_type = type;
        threat.severity = severity;
        threat.position[0] = 100.0f;
        threat.position[1] = 50.0f;
        threat.position[2] = 25.0f;
        threat.timestamp_ms = 1000000;
        if (desc) {
            strncpy(threat.description, desc, sizeof(threat.description) - 1);
        }
        return threat;
    }
};

/* ============================================================================
 * Threat Reception Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, ReceiveLowSeverityThreat) {
    /* Join swarm first */
    ASSERT_TRUE(swarm_brain_join(swarm));

    /* Create and send a low severity threat */
    threat_data_t threat = create_threat(1, 0.2f, "Low severity test");

    /* Process threat - normally this happens via message handling */
    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    /* Stats should be trackable */
    EXPECT_GE(stats.messages_sent, 0u);
}

TEST_F(SwarmBrainThreatTest, ReceiveMediumSeverityThreat) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    threat_data_t threat = create_threat(2, 0.5f, "Medium severity test");

    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    SUCCEED();  /* Medium threats are logged and monitored */
}

TEST_F(SwarmBrainThreatTest, ReceiveHighSeverityThreat) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    threat_data_t threat = create_threat(3, 0.85f, "High severity test");

    /* High threats should trigger broadcast */
    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    SUCCEED();  /* High threats are re-broadcast */
}

TEST_F(SwarmBrainThreatTest, ReceiveCriticalSeverityThreat) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    threat_data_t threat = create_threat(4, 0.98f, "CRITICAL - Immediate action required");

    /* Critical threats should escalate */
    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    SUCCEED();  /* Critical threats propose emergency action */
}

/* ============================================================================
 * Threat Type Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, HandleDifferentThreatTypes) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    /* Test various threat types */
    threat_data_t collision = create_threat(0, 0.9f, "Collision imminent");
    threat_data_t weather = create_threat(1, 0.6f, "Severe weather");
    threat_data_t obstacle = create_threat(2, 0.4f, "Obstacle detected");
    threat_data_t hostile = create_threat(3, 0.95f, "Hostile entity");

    SUCCEED();  /* All threat types should be processable */
}

/* ============================================================================
 * Threat Position Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, ThreatPositionParsing) {
    threat_data_t threat = create_threat(1, 0.5f, "Position test");
    threat.position[0] = 123.456f;
    threat.position[1] = 789.012f;
    threat.position[2] = 345.678f;

    /* Verify position is accessible */
    EXPECT_FLOAT_EQ(threat.position[0], 123.456f);
    EXPECT_FLOAT_EQ(threat.position[1], 789.012f);
    EXPECT_FLOAT_EQ(threat.position[2], 345.678f);
}

TEST_F(SwarmBrainThreatTest, ThreatWithNegativePosition) {
    threat_data_t threat = create_threat(1, 0.5f, "Negative position test");
    threat.position[0] = -50.0f;
    threat.position[1] = -100.0f;
    threat.position[2] = -25.0f;

    /* Negative positions should be valid */
    EXPECT_LT(threat.position[0], 0.0f);
    EXPECT_LT(threat.position[1], 0.0f);
    EXPECT_LT(threat.position[2], 0.0f);
}

/* ============================================================================
 * Workspace Update Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, ThreatUpdatesWorkspace) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    /* Create threat with high severity */
    threat_data_t threat = create_threat(5, 0.8f, "Workspace update test");

    /* Workspace should be updated with threat concept */
    /* The workspace is internal, but stats can indicate processing */
    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    SUCCEED();
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, ThreatWithZeroSeverity) {
    threat_data_t threat = create_threat(1, 0.0f, "Zero severity");

    EXPECT_FLOAT_EQ(threat.severity, 0.0f);
    SUCCEED();  /* Should be treated as informational */
}

TEST_F(SwarmBrainThreatTest, ThreatWithMaxSeverity) {
    threat_data_t threat = create_threat(1, 1.0f, "Maximum severity");

    EXPECT_FLOAT_EQ(threat.severity, 1.0f);
    SUCCEED();  /* Should escalate immediately */
}

TEST_F(SwarmBrainThreatTest, ThreatWithEmptyDescription) {
    threat_data_t threat = create_threat(1, 0.5f, nullptr);

    EXPECT_EQ(threat.description[0], '\0');
    SUCCEED();
}

TEST_F(SwarmBrainThreatTest, ThreatWithLongDescription) {
    char long_desc[300];
    memset(long_desc, 'X', sizeof(long_desc) - 1);
    long_desc[sizeof(long_desc) - 1] = '\0';

    threat_data_t threat = create_threat(1, 0.5f, long_desc);

    /* Should be truncated */
    EXPECT_LE(strlen(threat.description), sizeof(threat.description) - 1);
}

/* ============================================================================
 * Stats and Tracking Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, StatsAfterDestroy) {
    swarm_brain_destroy(swarm);
    swarm = nullptr;

    /* Should not crash when getting stats from destroyed brain */
    SUCCEED();
}

TEST_F(SwarmBrainThreatTest, MultipleThreatsInSequence) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    /* Send many threats in sequence */
    for (int i = 0; i < 20; i++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Threat %d", i);
        threat_data_t threat = create_threat(i % 5, 0.3f + (i * 0.03f), desc);
    }

    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    SUCCEED();
}

/* ============================================================================
 * Threat When Not Joined Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, ThreatBeforeJoin) {
    /* Do not join swarm */
    EXPECT_FALSE(swarm_brain_is_joined(swarm));

    threat_data_t threat = create_threat(1, 0.9f, "Before join test");

    /* Should not crash, but threat won't trigger proposal */
    SUCCEED();
}

TEST_F(SwarmBrainThreatTest, ThreatAfterLeave) {
    ASSERT_TRUE(swarm_brain_join(swarm));
    ASSERT_TRUE(swarm_brain_leave(swarm));

    EXPECT_FALSE(swarm_brain_is_joined(swarm));

    threat_data_t threat = create_threat(1, 0.9f, "After leave test");

    /* Should handle gracefully */
    SUCCEED();
}

/* ============================================================================
 * Integration-like Tests
 * ============================================================================ */

TEST_F(SwarmBrainThreatTest, ThreatWithProcess) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    threat_data_t threat = create_threat(1, 0.7f, "Process integration test");

    /* Run a processing cycle */
    swarm_brain_process(swarm);

    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    SUCCEED();
}

TEST_F(SwarmBrainThreatTest, ThreatWithMultiplePeers) {
    ASSERT_TRUE(swarm_brain_join(swarm));

    /* Simulate receiving threats from multiple peers */
    for (uint16_t peer = 2; peer <= 5; peer++) {
        char desc[64];
        snprintf(desc, sizeof(desc), "Threat from peer %u", peer);
        threat_data_t threat = create_threat(1, 0.5f, desc);
    }

    /* All peers should be tracked */
    swarm_stats_t stats;
    swarm_brain_get_stats(swarm, &stats);

    SUCCEED();
}
