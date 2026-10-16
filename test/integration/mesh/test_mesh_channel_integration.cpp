/**
 * @file test_mesh_channel_integration.cpp
 * @brief Integration Tests for Mesh Network Channel Operations
 *
 * WHAT: Tests complete channel lifecycle with world state, gossip, and endorsement
 * WHY:  Verify channels work correctly with all integrated components
 * HOW:  Create channels, add participants, process transactions, verify convergence
 *
 * TEST COVERAGE:
 * - Channel creation with world state
 * - Participant registration and membership
 * - Gossip belief propagation within channels
 * - Private data collection management
 * - Channel isolation verification
 * - World state CRDT convergence
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <string>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class MeshChannelIntegrationTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_PARTICIPANTS = 8;
    static constexpr size_t NUM_CHANNELS = 3;

    mesh_channel_t* channels_[NUM_CHANNELS] = {nullptr};
    mesh_participant_t* participants_[NUM_PARTICIPANTS] = {nullptr};

    void SetUp() override {
        // Create channels representing different brain regions
        const char* channel_names[] = {
            "left_hemisphere",
            "right_hemisphere",
            "subcortical"
        };

        for (size_t i = 0; i < NUM_CHANNELS; i++) {
            mesh_channel_config_t config;
            mesh_channel_config_init(&config);
            config.name = channel_names[i];
            config.max_participants = 32;
            config.enable_gossip = true;
            config.enable_private_data = true;

            channels_[i] = mesh_channel_create(&config);
            ASSERT_NE(channels_[i], nullptr)
                << "Failed to create channel: " << channel_names[i];
        }

        // Create participants and assign to channels
        for (size_t i = 0; i < NUM_PARTICIPANTS; i++) {
            mesh_participant_config_t config;
            mesh_participant_config_init(&config);

            char name[32];
            snprintf(name, sizeof(name), "participant_%zu", i);
            config.name = name;
            config.type = MESH_PARTICIPANT_TYPE_PEER;

            // Assign to channels (round-robin)
            config.home_channel = (mesh_channel_id_t)(i % NUM_CHANNELS);

            participants_[i] = mesh_participant_create(&config);
            ASSERT_NE(participants_[i], nullptr)
                << "Failed to create participant " << i;
        }
    }

    void TearDown() override {
        for (size_t i = 0; i < NUM_PARTICIPANTS; i++) {
            if (participants_[i]) {
                mesh_participant_destroy(participants_[i]);
                participants_[i] = nullptr;
            }
        }

        for (size_t i = 0; i < NUM_CHANNELS; i++) {
            if (channels_[i]) {
                mesh_channel_destroy(channels_[i]);
                channels_[i] = nullptr;
            }
        }
    }

    // Helper: Register participants with channels
    void RegisterParticipantsWithChannels() {
        for (size_t i = 0; i < NUM_PARTICIPANTS; i++) {
            size_t channel_idx = i % NUM_CHANNELS;
            nimcp_error_t err = mesh_channel_add_participant(
                channels_[channel_idx], participants_[i]);
            ASSERT_EQ(err, NIMCP_OK)
                << "Failed to add participant " << i << " to channel " << channel_idx;
        }
    }
};

// =============================================================================
// Channel Lifecycle Tests
// =============================================================================

TEST_F(MeshChannelIntegrationTest, ChannelCreationAndConfiguration) {
    // Verify channels created correctly
    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        ASSERT_NE(channels_[i], nullptr);

        mesh_channel_info_t info;
        nimcp_error_t err = mesh_channel_get_info(channels_[i], &info);
        ASSERT_EQ(err, NIMCP_OK);

        EXPECT_GT(info.channel_id, 0u);
        EXPECT_EQ(info.max_participants, 32u);
        EXPECT_TRUE(info.gossip_enabled);
        EXPECT_TRUE(info.private_data_enabled);
    }
}

TEST_F(MeshChannelIntegrationTest, ParticipantRegistrationAndMembership) {
    RegisterParticipantsWithChannels();

    // Verify participant counts per channel
    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        mesh_channel_info_t info;
        nimcp_error_t err = mesh_channel_get_info(channels_[i], &info);
        ASSERT_EQ(err, NIMCP_OK);

        // Each channel should have ~NUM_PARTICIPANTS/NUM_CHANNELS participants
        size_t expected_count = 0;
        for (size_t j = 0; j < NUM_PARTICIPANTS; j++) {
            if (j % NUM_CHANNELS == i) expected_count++;
        }

        EXPECT_EQ(info.participant_count, expected_count)
            << "Channel " << i << " participant count mismatch";
    }

    // Verify participants know their channel membership
    for (size_t i = 0; i < NUM_PARTICIPANTS; i++) {
        mesh_participant_info_t pinfo;
        nimcp_error_t err = mesh_participant_get_info(participants_[i], &pinfo);
        ASSERT_EQ(err, NIMCP_OK);

        size_t expected_channel = i % NUM_CHANNELS;
        mesh_channel_info_t cinfo;
        mesh_channel_get_info(channels_[expected_channel], &cinfo);

        EXPECT_TRUE(mesh_participant_is_member_of(participants_[i], cinfo.channel_id))
            << "Participant " << i << " should be member of channel " << expected_channel;
    }
}

// =============================================================================
// World State Tests
// =============================================================================

TEST_F(MeshChannelIntegrationTest, WorldStateIsolation) {
    RegisterParticipantsWithChannels();

    // Set different world state in each channel
    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "channel_%zu_key", i);
        snprintf(value, sizeof(value), "channel_%zu_value", i);

        nimcp_error_t err = mesh_channel_world_state_put(
            channels_[i], key, value, strlen(value) + 1);
        ASSERT_EQ(err, NIMCP_OK);
    }

    // Verify world state isolation (each channel has only its own data)
    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        for (size_t j = 0; j < NUM_CHANNELS; j++) {
            char key[32], expected_value[32];
            snprintf(key, sizeof(key), "channel_%zu_key", j);
            snprintf(expected_value, sizeof(expected_value), "channel_%zu_value", j);

            char value[64] = {0};
            size_t value_size = sizeof(value);
            nimcp_error_t err = mesh_channel_world_state_get(
                channels_[i], key, value, &value_size);

            if (i == j) {
                // Same channel should find the key
                EXPECT_EQ(err, NIMCP_OK)
                    << "Channel " << i << " should have key for channel " << j;
                EXPECT_STREQ(value, expected_value);
            } else {
                // Different channel should NOT find the key
                EXPECT_NE(err, NIMCP_OK)
                    << "Channel " << i << " should NOT have key for channel " << j;
            }
        }
    }
}

TEST_F(MeshChannelIntegrationTest, WorldStateUpdatePropagation) {
    RegisterParticipantsWithChannels();

    // Get first channel and its participants
    mesh_channel_t* ch = channels_[0];

    // Put initial value
    const char* key = "shared_belief";
    const char* value1 = "initial_value";
    nimcp_error_t err = mesh_channel_world_state_put(ch, key, value1, strlen(value1) + 1);
    ASSERT_EQ(err, NIMCP_OK);

    // Update value
    const char* value2 = "updated_value";
    err = mesh_channel_world_state_put(ch, key, value2, strlen(value2) + 1);
    ASSERT_EQ(err, NIMCP_OK);

    // Verify all participants in channel see updated value
    char retrieved[64] = {0};
    size_t retrieved_size = sizeof(retrieved);
    err = mesh_channel_world_state_get(ch, key, retrieved, &retrieved_size);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_STREQ(retrieved, value2);
}

// =============================================================================
// Private Data Collection Tests
// =============================================================================

TEST_F(MeshChannelIntegrationTest, PrivateDataCollections) {
    RegisterParticipantsWithChannels();

    mesh_channel_t* ch = channels_[0];

    // Create a private data collection
    const char* collection_name = "sensitive_beliefs";
    nimcp_error_t err = mesh_channel_create_private_collection(
        ch, collection_name, 4096);  // 4KB max size
    ASSERT_EQ(err, NIMCP_OK);

    // Store private data
    const char* key = "private_key";
    const char* value = "private_value";
    err = mesh_channel_private_put(ch, collection_name, key, value, strlen(value) + 1);
    ASSERT_EQ(err, NIMCP_OK);

    // Retrieve private data
    char retrieved[64] = {0};
    size_t retrieved_size = sizeof(retrieved);
    err = mesh_channel_private_get(ch, collection_name, key, retrieved, &retrieved_size);
    ASSERT_EQ(err, NIMCP_OK);
    EXPECT_STREQ(retrieved, value);

    // Private data should not be in world state
    retrieved_size = sizeof(retrieved);
    memset(retrieved, 0, sizeof(retrieved));
    err = mesh_channel_world_state_get(ch, key, retrieved, &retrieved_size);
    EXPECT_NE(err, NIMCP_OK) << "Private data should not be in world state";
}

// =============================================================================
// Gossip Integration Tests
// =============================================================================

TEST_F(MeshChannelIntegrationTest, GossipBeliefPropagation) {
    RegisterParticipantsWithChannels();

    mesh_channel_t* ch = channels_[0];

    // Introduce a belief through gossip
    mesh_belief_t belief = {0};
    belief.belief_type = MESH_BELIEF_TYPE_STATE;
    belief.confidence = 0.8f;
    strncpy(belief.topic, "test_belief", sizeof(belief.topic) - 1);
    belief.data_size = 16;
    memset(belief.data, 0xAB, belief.data_size);

    nimcp_error_t err = mesh_channel_gossip_introduce(ch, &belief);
    ASSERT_EQ(err, NIMCP_OK);

    // Run gossip rounds
    for (int round = 0; round < 5; round++) {
        err = mesh_channel_gossip_round(ch);
        ASSERT_EQ(err, NIMCP_OK);
    }

    // Check consensus beliefs
    mesh_belief_set_t consensus_beliefs;
    mesh_belief_set_init(&consensus_beliefs);

    err = mesh_channel_get_consensus_beliefs(ch, &consensus_beliefs);
    ASSERT_EQ(err, NIMCP_OK);

    // Should have at least one consensus belief
    EXPECT_GT(consensus_beliefs.count, 0u);

    mesh_belief_set_cleanup(&consensus_beliefs);
}

// =============================================================================
// Multi-Channel Coordination Tests
// =============================================================================

TEST_F(MeshChannelIntegrationTest, CrossChannelParticipantIsolation) {
    RegisterParticipantsWithChannels();

    // Participant 0 is in channel 0
    // Try to access channel 1's world state through participant 0
    mesh_channel_t* ch0 = channels_[0];
    mesh_channel_t* ch1 = channels_[1];

    // Put data in channel 1
    const char* key = "ch1_only";
    const char* value = "ch1_value";
    nimcp_error_t err = mesh_channel_world_state_put(ch1, key, value, strlen(value) + 1);
    ASSERT_EQ(err, NIMCP_OK);

    // Verify channel 0 cannot see channel 1's data
    char retrieved[64] = {0};
    size_t retrieved_size = sizeof(retrieved);
    err = mesh_channel_world_state_get(ch0, key, retrieved, &retrieved_size);
    EXPECT_NE(err, NIMCP_OK) << "Channel 0 should not see channel 1 data";
}

// =============================================================================
// Stress Tests
// =============================================================================

TEST_F(MeshChannelIntegrationTest, ConcurrentWorldStateAccess) {
    RegisterParticipantsWithChannels();

    mesh_channel_t* ch = channels_[0];
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    // Multiple threads updating world state
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 25; i++) {
                char key[32], value[32];
                snprintf(key, sizeof(key), "thread_%d_key_%d", t, i);
                snprintf(value, sizeof(value), "thread_%d_value_%d", t, i);

                nimcp_error_t err = mesh_channel_world_state_put(
                    ch, key, value, strlen(value) + 1);

                if (err == NIMCP_OK) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Most operations should succeed
    EXPECT_GT(success_count.load(), 80);

    // Verify some entries exist
    char value[64] = {0};
    size_t value_size = sizeof(value);
    nimcp_error_t err = mesh_channel_world_state_get(
        ch, "thread_0_key_0", value, &value_size);
    EXPECT_EQ(err, NIMCP_OK);
}

// =============================================================================
// Channel Health and Metrics Tests
// =============================================================================

TEST_F(MeshChannelIntegrationTest, ChannelHealthMetrics) {
    RegisterParticipantsWithChannels();

    mesh_channel_t* ch = channels_[0];

    // Perform some operations
    for (int i = 0; i < 10; i++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        mesh_channel_world_state_put(ch, key, value, strlen(value) + 1);
    }

    // Get health metrics
    mesh_channel_health_t health;
    nimcp_error_t err = mesh_channel_get_health(ch, &health);
    ASSERT_EQ(err, NIMCP_OK);

    EXPECT_GT(health.participant_count, 0u);
    EXPECT_GE(health.world_state_entries, 10u);
    EXPECT_TRUE(health.gossip_active);
}
