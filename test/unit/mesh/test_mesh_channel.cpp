/**
 * @file test_mesh_channel.cpp
 * @brief Unit tests for mesh channel module
 *
 * Tests channel creation, participant management, world state,
 * private data collections, and gossip/convergence.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_participant.h"
}

class MeshChannelTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry;
    mesh_channel_t* channel;

    void SetUp() override {
        registry = mesh_registry_create(nullptr);
        ASSERT_NE(registry, nullptr);
        channel = nullptr;
    }

    void TearDown() override {
        if (channel) {
            mesh_channel_destroy(channel);
            channel = nullptr;
        }
        if (registry) {
            mesh_registry_destroy(registry);
            registry = nullptr;
        }
    }

    mesh_participant_id_t register_test_participant(const char* name,
                                                     mesh_channel_id_t home_channel) {
        mesh_participant_interface_t iface;
        mesh_participant_interface_init(&iface);

        mesh_participant_config_t config;
        mesh_participant_config_init(&config);
        config.module_name = name;
        config.type = MESH_PARTICIPANT_MODULE;
        config.home_channel = home_channel;

        mesh_participant_id_t id;
        nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &id);
        EXPECT_EQ(err, NIMCP_SUCCESS);
        return id;
    }
};

/* ============================================================================
 * Channel Creation Tests
 * ============================================================================ */

TEST_F(MeshChannelTest, CreateWithDefaults) {
    mesh_channel_config_t config;
    nimcp_error_t err = mesh_channel_default_config(&config);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    config.channel_name = "test_channel";
    config.channel_id = 100;

    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    EXPECT_EQ(mesh_channel_get_id(channel), 100);
    EXPECT_STREQ(mesh_channel_get_name(channel), "test_channel");
}

TEST_F(MeshChannelTest, CreateWithNullConfig) {
    channel = mesh_channel_create(nullptr, registry);
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(mesh_channel_get_id(channel), 0);
}

TEST_F(MeshChannelTest, DestroyNull) {
    mesh_channel_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Participant Management Tests
 * ============================================================================ */

TEST_F(MeshChannelTest, AddParticipant) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_name = "test";
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    nimcp_error_t err = mesh_channel_add_participant(channel, p1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_TRUE(mesh_channel_has_participant(channel, p1));
    EXPECT_EQ(mesh_channel_get_participant_count(channel), 1);
}

TEST_F(MeshChannelTest, AddDuplicateParticipant) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    mesh_channel_add_participant(channel, p1);
    nimcp_error_t err = mesh_channel_add_participant(channel, p1);

    EXPECT_EQ(err, NIMCP_SUCCESS); // Duplicate is OK
    EXPECT_EQ(mesh_channel_get_participant_count(channel), 1);
}

TEST_F(MeshChannelTest, RemoveParticipant) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);
    mesh_channel_add_participant(channel, p1);

    nimcp_error_t err = mesh_channel_remove_participant(channel, p1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FALSE(mesh_channel_has_participant(channel, p1));
    EXPECT_EQ(mesh_channel_get_participant_count(channel), 0);
}

TEST_F(MeshChannelTest, RemoveNonexistentParticipant) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    nimcp_error_t err = mesh_channel_remove_participant(channel, 0xDEADBEEF);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

TEST_F(MeshChannelTest, GetParticipants) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);
    mesh_participant_id_t p2 = register_test_participant("module2", 1);

    mesh_channel_add_participant(channel, p1);
    mesh_channel_add_participant(channel, p2);

    mesh_participant_id_t ids[10];
    size_t count;
    nimcp_error_t err = mesh_channel_get_participants(channel, ids, 10, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 2);
}

/* ============================================================================
 * World State Tests
 * ============================================================================ */

TEST_F(MeshChannelTest, WorldStateExists) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    collective_workspace_t* ws = mesh_channel_get_world_state(channel);
    EXPECT_NE(ws, nullptr);
}

TEST_F(MeshChannelTest, AddWorldStateItem) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    float content[16] = {1.0f, 2.0f, 3.0f, 4.0f};
    nimcp_error_t err = mesh_channel_add_world_state_item(
        channel, p1, 1, content, 16, 0.8f);

    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshChannelTest, WorldStateCoherence) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    float coherence = mesh_channel_get_world_state_coherence(channel);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

/* ============================================================================
 * Private Data Collection Tests
 * ============================================================================ */

TEST_F(MeshChannelTest, CreatePrivateCollection) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    nimcp_error_t err = mesh_channel_create_private_collection(
        channel, "secrets", p1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshChannelTest, CreateDuplicateCollection) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    mesh_channel_create_private_collection(channel, "secrets", p1);
    nimcp_error_t err = mesh_channel_create_private_collection(
        channel, "secrets", p1);
    EXPECT_EQ(err, NIMCP_ERROR_ALREADY_EXISTS);
}

TEST_F(MeshChannelTest, PutAndGetPrivateData) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    mesh_channel_create_private_collection(channel, "data", p1);

    const char* value = "secret_value";
    nimcp_error_t err = mesh_channel_put_private_data(
        channel, "data", p1, "key1", value, strlen(value) + 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    char buffer[256];
    size_t len;
    err = mesh_channel_get_private_data(
        channel, "data", p1, "key1", buffer, &len, sizeof(buffer));
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_STREQ(buffer, "secret_value");
}

TEST_F(MeshChannelTest, UnauthorizedAccess) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);
    mesh_participant_id_t p2 = register_test_participant("module2", 1);

    mesh_channel_create_private_collection(channel, "private", p1);

    // p2 should not be able to access
    char buffer[256];
    size_t len;
    nimcp_error_t err = mesh_channel_get_private_data(
        channel, "private", p2, "key", buffer, &len, sizeof(buffer));
    EXPECT_EQ(err, NIMCP_ERROR_PERMISSION_DENIED);
}

TEST_F(MeshChannelTest, AuthorizeForCollection) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);
    mesh_participant_id_t p2 = register_test_participant("module2", 1);

    mesh_channel_create_private_collection(channel, "shared", p1);

    // Authorize p2
    nimcp_error_t err = mesh_channel_authorize_for_collection(
        channel, "shared", p1, p2);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // p2 should now be able to put data
    const char* value = "from_p2";
    err = mesh_channel_put_private_data(
        channel, "shared", p2, "key", value, strlen(value) + 1);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshChannelTest, DeletePrivateData) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    mesh_channel_create_private_collection(channel, "data", p1);
    mesh_channel_put_private_data(channel, "data", p1, "key", "val", 4);

    nimcp_error_t err = mesh_channel_delete_private_data(
        channel, "data", p1, "key");
    EXPECT_EQ(err, NIMCP_SUCCESS);

    char buffer[256];
    size_t len;
    err = mesh_channel_get_private_data(
        channel, "data", p1, "key", buffer, &len, sizeof(buffer));
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
}

/* ============================================================================
 * Gossip and Belief Tests
 * ============================================================================ */

TEST_F(MeshChannelTest, IntroduceBelief) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 1;
    belief.source = p1;
    belief.channel = 1;
    belief.certainty = 0.9f;
    belief.vector_dim = 16;
    for (size_t i = 0; i < 16; i++) {
        belief.belief_vector[i] = (float)i / 16.0f;
    }

    nimcp_error_t err = mesh_channel_introduce_belief(channel, &belief);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshChannelTest, GossipRound) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 1;
    belief.source = p1;
    belief.certainty = 0.9f;
    belief.vector_dim = 4;

    mesh_channel_introduce_belief(channel, &belief);

    nimcp_error_t err = mesh_channel_gossip_round(channel);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshChannelTest, GetConsensusBeliefs) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_participant_id_t p1 = register_test_participant("module1", 1);

    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 1;
    belief.source = p1;
    belief.certainty = 0.8f;

    mesh_channel_introduce_belief(channel, &belief);

    mesh_belief_t beliefs[10];
    size_t count;
    nimcp_error_t err = mesh_channel_get_consensus_beliefs(
        channel, beliefs, 10, &count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(count, 1);
}

/* ============================================================================
 * Convergence Tests
 * ============================================================================ */

TEST_F(MeshChannelTest, InitialFreeEnergy) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    float fe = mesh_channel_get_free_energy(channel);
    EXPECT_GE(fe, 0.0f);
    EXPECT_LE(fe, 1.0f);
}

TEST_F(MeshChannelTest, ConvergenceProgress) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    float progress = mesh_channel_get_convergence_progress(channel);
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
}

TEST_F(MeshChannelTest, UpdateChannel) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    nimcp_error_t err = mesh_channel_update(channel, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshChannelTest, HasConvergedInitially) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    // Initially not converged (no activity)
    EXPECT_FALSE(mesh_channel_has_converged(channel));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(MeshChannelTest, GetStats) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 42;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_channel_stats_t stats;
    nimcp_error_t err = mesh_channel_get_stats(channel, &stats);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(stats.channel_id, 42);
}

TEST_F(MeshChannelTest, ResetStats) {
    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_id = 1;
    channel = mesh_channel_create(&config, registry);
    ASSERT_NE(channel, nullptr);

    mesh_channel_add_participant(channel,
        register_test_participant("m1", 1));

    mesh_channel_reset_stats(channel);

    mesh_channel_stats_t stats;
    mesh_channel_get_stats(channel, &stats);
    EXPECT_EQ(stats.transactions_processed, 0);
}

/* ============================================================================
 * Channel Manager Tests
 * ============================================================================ */

class MeshChannelManagerTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry;
    mesh_channel_manager_t* manager;

    void SetUp() override {
        registry = mesh_registry_create(nullptr);
        ASSERT_NE(registry, nullptr);
        manager = nullptr;
    }

    void TearDown() override {
        if (manager) {
            mesh_channel_manager_destroy(manager);
            manager = nullptr;
        }
        if (registry) {
            mesh_registry_destroy(registry);
            registry = nullptr;
        }
    }
};

TEST_F(MeshChannelManagerTest, CreateManager) {
    mesh_channel_manager_config_t config = {
        .max_channels = 10,
        .enable_logging = true
    };

    manager = mesh_channel_manager_create(&config, registry);
    ASSERT_NE(manager, nullptr);
}

TEST_F(MeshChannelManagerTest, CreateWithNullConfig) {
    manager = mesh_channel_manager_create(nullptr, registry);
    ASSERT_NE(manager, nullptr);
}

TEST_F(MeshChannelManagerTest, CreateChannel) {
    manager = mesh_channel_manager_create(nullptr, registry);
    ASSERT_NE(manager, nullptr);

    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_name = "test";
    config.channel_id = 10;

    mesh_channel_t* ch = mesh_channel_manager_create_channel(manager, &config);
    ASSERT_NE(ch, nullptr);
    EXPECT_EQ(mesh_channel_get_id(ch), 10);
}

TEST_F(MeshChannelManagerTest, GetChannelById) {
    manager = mesh_channel_manager_create(nullptr, registry);
    ASSERT_NE(manager, nullptr);

    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_name = "test";
    config.channel_id = 25;

    mesh_channel_manager_create_channel(manager, &config);

    mesh_channel_t* ch = mesh_channel_manager_get_channel(manager, 25);
    ASSERT_NE(ch, nullptr);
    EXPECT_STREQ(mesh_channel_get_name(ch), "test");
}

TEST_F(MeshChannelManagerTest, GetChannelByName) {
    manager = mesh_channel_manager_create(nullptr, registry);
    ASSERT_NE(manager, nullptr);

    mesh_channel_config_t config;
    mesh_channel_default_config(&config);
    config.channel_name = "mytest";
    config.channel_id = 30;

    mesh_channel_manager_create_channel(manager, &config);

    mesh_channel_t* ch = mesh_channel_manager_get_channel_by_name(manager, "mytest");
    ASSERT_NE(ch, nullptr);
    EXPECT_EQ(mesh_channel_get_id(ch), 30);
}

TEST_F(MeshChannelManagerTest, GetNonexistentChannel) {
    manager = mesh_channel_manager_create(nullptr, registry);
    ASSERT_NE(manager, nullptr);

    mesh_channel_t* ch = mesh_channel_manager_get_channel(manager, 999);
    EXPECT_EQ(ch, nullptr);
}

TEST_F(MeshChannelManagerTest, CreateStandardChannels) {
    manager = mesh_channel_manager_create(nullptr, registry);
    ASSERT_NE(manager, nullptr);

    nimcp_error_t err = mesh_channel_manager_create_standard_channels(manager);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Verify all 5 standard channels exist
    EXPECT_NE(mesh_channel_manager_get_channel(manager, MESH_CHANNEL_SYSTEM), nullptr);
    EXPECT_NE(mesh_channel_manager_get_channel(manager, MESH_CHANNEL_LEFT_HEMISPHERE), nullptr);
    EXPECT_NE(mesh_channel_manager_get_channel(manager, MESH_CHANNEL_RIGHT_HEMISPHERE), nullptr);
    EXPECT_NE(mesh_channel_manager_get_channel(manager, MESH_CHANNEL_SUBCORTICAL), nullptr);
    EXPECT_NE(mesh_channel_manager_get_channel(manager, MESH_CHANNEL_GPU_COMPUTE), nullptr);
}

TEST_F(MeshChannelManagerTest, UpdateAllChannels) {
    manager = mesh_channel_manager_create(nullptr, registry);
    ASSERT_NE(manager, nullptr);

    mesh_channel_manager_create_standard_channels(manager);

    nimcp_error_t err = mesh_channel_manager_update(manager, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MeshChannelManagerTest, DestroyNull) {
    mesh_channel_manager_destroy(nullptr);
    // Should not crash
}
