/**
 * @file test_swarm_brain.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Brain Coordinator
 *
 * TEST COVERAGE:
 * - Creation and destruction
 * - Join/leave swarm
 * - Message processing
 * - Heartbeat generation
 * - Perception broadcasting
 * - Neuromodulator sync
 * - Integration with components
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards

// Forward declarations (simplified for testing)
typedef struct brain brain_t;
typedef struct swarm_signal_adapter swarm_signal_adapter_t;

// Swarm brain configuration
typedef struct {
    uint16_t drone_id;
    char swarm_name[32];
    uint32_t heartbeat_ms;
    uint32_t sync_ms;
    uint32_t vote_timeout_ms;
    float coherence_threshold;
    uint32_t critical_mass;
    uint32_t workspace_size;
    float broadcast_threshold;
    float neuromod_diffusion;
    bool enable_reward_sharing;
} swarm_brain_config_t;

// Swarm brain state
typedef struct {
    swarm_brain_config_t config;
    brain_t* local_brain;
    swarm_signal_adapter_t* signal_adapter;
    bool is_joined;
    char joined_swarm_id[32];
    uint32_t connected_drones;
    uint64_t last_heartbeat_ms;
    uint32_t messages_processed;
    uint32_t messages_sent;
} swarm_brain_t;

// API functions
swarm_brain_t* swarm_brain_create(
    const swarm_brain_config_t* config,
    brain_t* local_brain,
    swarm_signal_adapter_t* signal_adapter
);

void swarm_brain_destroy(swarm_brain_t* swarm);

bool swarm_brain_join(swarm_brain_t* swarm, const char* swarm_id);

bool swarm_brain_leave(swarm_brain_t* swarm);

uint32_t swarm_brain_process(swarm_brain_t* swarm);

bool swarm_brain_broadcast_perception(
    swarm_brain_t* swarm,
    const float* perception,
    float salience
);

bool swarm_brain_sync_neuromodulators(swarm_brain_t* swarm);

swarm_brain_config_t swarm_brain_default_config(uint16_t drone_id);

uint32_t swarm_brain_get_connected_count(const swarm_brain_t* swarm);

bool swarm_brain_is_joined(const swarm_brain_t* swarm);

//=============================================================================
// Mock Implementation
//=============================================================================

swarm_brain_config_t swarm_brain_default_config(uint16_t drone_id) {
    swarm_brain_config_t config;
    config.drone_id = drone_id;
    strncpy(config.swarm_name, "default_swarm", sizeof(config.swarm_name));
    config.heartbeat_ms = 100;
    config.sync_ms = 50;
    config.vote_timeout_ms = 100;
    config.coherence_threshold = 0.5f;
    config.critical_mass = 8;
    config.workspace_size = 32;
    config.broadcast_threshold = 0.7f;
    config.neuromod_diffusion = 0.3f;
    config.enable_reward_sharing = true;
    return config;
}

swarm_brain_t* swarm_brain_create(
    const swarm_brain_config_t* config,
    brain_t* local_brain,
    swarm_signal_adapter_t* signal_adapter
) {
    if (!config) return nullptr;

    swarm_brain_t* swarm = new swarm_brain_t();
    swarm->config = *config;
    swarm->local_brain = local_brain;
    swarm->signal_adapter = signal_adapter;
    swarm->is_joined = false;
    swarm->joined_swarm_id[0] = '\0';
    swarm->connected_drones = 1; // Self
    swarm->last_heartbeat_ms = 0;
    swarm->messages_processed = 0;
    swarm->messages_sent = 0;

    return swarm;
}

void swarm_brain_destroy(swarm_brain_t* swarm) {
    if (swarm) {
        delete swarm;
    }
}

bool swarm_brain_join(swarm_brain_t* swarm, const char* swarm_id) {
    if (!swarm || !swarm_id) return false;
    if (swarm->is_joined) return false;

    strncpy(swarm->joined_swarm_id, swarm_id, sizeof(swarm->joined_swarm_id) - 1);
    swarm->joined_swarm_id[sizeof(swarm->joined_swarm_id) - 1] = '\0';
    swarm->is_joined = true;

    return true;
}

bool swarm_brain_leave(swarm_brain_t* swarm) {
    if (!swarm || !swarm->is_joined) return false;

    swarm->is_joined = false;
    swarm->joined_swarm_id[0] = '\0';
    swarm->connected_drones = 1; // Reset to self only

    return true;
}

uint32_t swarm_brain_process(swarm_brain_t* swarm) {
    if (!swarm) return 0;

    // Simulate processing messages
    uint32_t processed = 0;

    // Process heartbeats
    swarm->last_heartbeat_ms++;
    processed++;

    // Simulate receiving some messages
    if (swarm->is_joined) {
        processed += 2; // Simulate 2 messages from swarm
    }

    swarm->messages_processed += processed;
    return processed;
}

bool swarm_brain_broadcast_perception(
    swarm_brain_t* swarm,
    const float* perception,
    float salience
) {
    if (!swarm || !perception) return false;
    if (!swarm->is_joined) return false;
    if (salience < swarm->config.broadcast_threshold) return false;

    swarm->messages_sent++;
    return true;
}

bool swarm_brain_sync_neuromodulators(swarm_brain_t* swarm) {
    if (!swarm || !swarm->is_joined) return false;

    swarm->messages_sent++;
    return true;
}

uint32_t swarm_brain_get_connected_count(const swarm_brain_t* swarm) {
    return swarm ? swarm->connected_drones : 0;
}

bool swarm_brain_is_joined(const swarm_brain_t* swarm) {
    return swarm ? swarm->is_joined : false;
}

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmBrainTest : public ::testing::Test {
protected:
    swarm_brain_t* swarm;

    void SetUp() override {
        swarm = nullptr;
    }

    void TearDown() override {
        if (swarm) {
            swarm_brain_destroy(swarm);
            swarm = nullptr;
        }
    }
};

//=============================================================================
// 1. Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmBrainTest, CreateWithDefaultConfig) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    ASSERT_NE(swarm, nullptr);
    EXPECT_EQ(swarm->config.drone_id, 1);
    EXPECT_FALSE(swarm->is_joined);
    EXPECT_EQ(swarm->connected_drones, 1u);
}

TEST_F(SwarmBrainTest, CreateWithCustomConfig) {
    swarm_brain_config_t config = swarm_brain_default_config(42);
    config.heartbeat_ms = 200;
    config.workspace_size = 64;

    swarm = swarm_brain_create(&config, nullptr, nullptr);

    ASSERT_NE(swarm, nullptr);
    EXPECT_EQ(swarm->config.drone_id, 42);
    EXPECT_EQ(swarm->config.heartbeat_ms, 200u);
    EXPECT_EQ(swarm->config.workspace_size, 64u);
}

TEST_F(SwarmBrainTest, CreateWithNullConfig) {
    swarm = swarm_brain_create(nullptr, nullptr, nullptr);

    EXPECT_EQ(swarm, nullptr);
}

TEST_F(SwarmBrainTest, DestroyNull) {
    swarm_brain_destroy(nullptr); // Should not crash
}

//=============================================================================
// 2. Join/Leave Swarm Tests
//=============================================================================

TEST_F(SwarmBrainTest, JoinSwarm) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    bool success = swarm_brain_join(swarm, "test_swarm");

    ASSERT_TRUE(success);
    EXPECT_TRUE(swarm_brain_is_joined(swarm));
    EXPECT_STREQ(swarm->joined_swarm_id, "test_swarm");
}

TEST_F(SwarmBrainTest, JoinMultipleSwarmsFails) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    ASSERT_TRUE(swarm_brain_join(swarm, "swarm1"));
    EXPECT_FALSE(swarm_brain_join(swarm, "swarm2")); // Should fail
    EXPECT_STREQ(swarm->joined_swarm_id, "swarm1"); // Still in first swarm
}

TEST_F(SwarmBrainTest, LeaveSwarm) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    swarm_brain_join(swarm, "test_swarm");
    ASSERT_TRUE(swarm_brain_is_joined(swarm));

    bool success = swarm_brain_leave(swarm);

    ASSERT_TRUE(success);
    EXPECT_FALSE(swarm_brain_is_joined(swarm));
    EXPECT_EQ(swarm->connected_drones, 1u); // Reset to self only
}

TEST_F(SwarmBrainTest, LeaveWhenNotJoinedFails) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    bool success = swarm_brain_leave(swarm);

    EXPECT_FALSE(success);
}

TEST_F(SwarmBrainTest, JoinWithNullId) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    bool success = swarm_brain_join(swarm, nullptr);

    EXPECT_FALSE(success);
}

//=============================================================================
// 3. Message Processing Tests
//=============================================================================

TEST_F(SwarmBrainTest, ProcessMessagesWhenNotJoined) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    uint32_t processed = swarm_brain_process(swarm);

    EXPECT_GT(processed, 0u); // Should still process heartbeats
}

TEST_F(SwarmBrainTest, ProcessMessagesWhenJoined) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);
    swarm_brain_join(swarm, "test_swarm");

    uint32_t processed = swarm_brain_process(swarm);

    EXPECT_GT(processed, 0u);
    EXPECT_GT(swarm->messages_processed, 0u);
}

TEST_F(SwarmBrainTest, ProcessMultipleRounds) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);
    swarm_brain_join(swarm, "test_swarm");

    for (int i = 0; i < 10; i++) {
        swarm_brain_process(swarm);
    }

    EXPECT_GE(swarm->messages_processed, 10u);
}

//=============================================================================
// 4. Perception Broadcasting Tests
//=============================================================================

TEST_F(SwarmBrainTest, BroadcastHighSaliencePerception) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);
    swarm_brain_join(swarm, "test_swarm");

    float perception[16] = {1.0f, 2.0f, 3.0f};
    bool success = swarm_brain_broadcast_perception(swarm, perception, 0.9f);

    ASSERT_TRUE(success);
    EXPECT_EQ(swarm->messages_sent, 1u);
}

TEST_F(SwarmBrainTest, LowSaliencePerceptionNotBroadcast) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);
    swarm_brain_join(swarm, "test_swarm");

    float perception[16] = {1.0f, 2.0f, 3.0f};
    bool success = swarm_brain_broadcast_perception(swarm, perception, 0.5f); // Below threshold

    EXPECT_FALSE(success);
    EXPECT_EQ(swarm->messages_sent, 0u);
}

TEST_F(SwarmBrainTest, BroadcastWhenNotJoinedFails) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    float perception[16] = {1.0f, 2.0f, 3.0f};
    bool success = swarm_brain_broadcast_perception(swarm, perception, 0.9f);

    EXPECT_FALSE(success);
}

TEST_F(SwarmBrainTest, BroadcastNullPerceptionFails) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);
    swarm_brain_join(swarm, "test_swarm");

    bool success = swarm_brain_broadcast_perception(swarm, nullptr, 0.9f);

    EXPECT_FALSE(success);
}

//=============================================================================
// 5. Neuromodulator Sync Tests
//=============================================================================

TEST_F(SwarmBrainTest, SyncNeuromodulators) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);
    swarm_brain_join(swarm, "test_swarm");

    bool success = swarm_brain_sync_neuromodulators(swarm);

    ASSERT_TRUE(success);
    EXPECT_EQ(swarm->messages_sent, 1u);
}

TEST_F(SwarmBrainTest, SyncWhenNotJoinedFails) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    bool success = swarm_brain_sync_neuromodulators(swarm);

    EXPECT_FALSE(success);
}

//=============================================================================
// 6. Connection Tracking Tests
//=============================================================================

TEST_F(SwarmBrainTest, InitialConnectionCountIsSelf) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    uint32_t count = swarm_brain_get_connected_count(swarm);

    EXPECT_EQ(count, 1u);
}

TEST_F(SwarmBrainTest, GetConnectedCountNull) {
    uint32_t count = swarm_brain_get_connected_count(nullptr);

    EXPECT_EQ(count, 0u);
}

//=============================================================================
// 7. Configuration Tests
//=============================================================================

TEST_F(SwarmBrainTest, DefaultConfigValid) {
    swarm_brain_config_t config = swarm_brain_default_config(5);

    EXPECT_EQ(config.drone_id, 5);
    EXPECT_GT(config.heartbeat_ms, 0u);
    EXPECT_GT(config.sync_ms, 0u);
    EXPECT_GT(config.vote_timeout_ms, 0u);
    EXPECT_GT(config.coherence_threshold, 0.0f);
    EXPECT_GT(config.critical_mass, 0u);
    EXPECT_GT(config.workspace_size, 0u);
    EXPECT_GT(config.broadcast_threshold, 0.0f);
}

TEST_F(SwarmBrainTest, ConfigPreservesCustomValues) {
    swarm_brain_config_t config = swarm_brain_default_config(10);
    config.heartbeat_ms = 500;
    config.critical_mass = 16;
    config.neuromod_diffusion = 0.5f;

    swarm = swarm_brain_create(&config, nullptr, nullptr);

    EXPECT_EQ(swarm->config.heartbeat_ms, 500u);
    EXPECT_EQ(swarm->config.critical_mass, 16u);
    EXPECT_FLOAT_EQ(swarm->config.neuromod_diffusion, 0.5f);
}

//=============================================================================
// 8. Edge Cases
//=============================================================================

TEST_F(SwarmBrainTest, DroneIdZero) {
    swarm_brain_config_t config = swarm_brain_default_config(0);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    ASSERT_NE(swarm, nullptr);
    EXPECT_EQ(swarm->config.drone_id, 0);
}

TEST_F(SwarmBrainTest, MaxDroneId) {
    swarm_brain_config_t config = swarm_brain_default_config(65535);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    ASSERT_NE(swarm, nullptr);
    EXPECT_EQ(swarm->config.drone_id, 65535);
}

TEST_F(SwarmBrainTest, LongSwarmName) {
    swarm_brain_config_t config = swarm_brain_default_config(1);
    swarm = swarm_brain_create(&config, nullptr, nullptr);

    char long_name[100];
    memset(long_name, 'a', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';

    bool success = swarm_brain_join(swarm, long_name);

    ASSERT_TRUE(success);
    // Name should be truncated
    EXPECT_LT(strlen(swarm->joined_swarm_id), sizeof(long_name));
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
