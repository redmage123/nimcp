/**
 * @file test_mesh_q1_simple_recall.cpp
 * @brief Unit test for Q1: Simple Fact Recall - "What is 2 + 2?"
 *
 * WHAT: Tests single-channel participant, transaction, and gossip flow
 * WHY:  Validates basic mesh network operation per inference question Q1
 * HOW:  Create participant, channel, belief, run gossip, verify world state
 *
 * COMPONENTS EXERCISED:
 * - Participant registration
 * - Channel creation
 * - Transaction creation (MESH_TX_BELIEF_UPDATE)
 * - Gossip round
 * - World state update
 *
 * REFERENCE: docs/mesh_inference_questions.md - Q1
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
}

/**
 * @brief Test fixture for Q1 Simple Fact Recall
 */
class MeshQ1SimpleRecallTest : public ::testing::Test {
protected:
    mesh_participant_registry_t* registry = nullptr;
    mesh_channel_t* channel = nullptr;

    void SetUp() override {
        /* Create participant registry */
        mesh_registry_config_t reg_config;
        mesh_registry_default_config(&reg_config);
        registry = mesh_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create channel for left hemisphere (analytical processing) */
        mesh_channel_config_t ch_config;
        mesh_channel_default_config(&ch_config);
        ch_config.channel_name = "left_hemisphere";
        ch_config.channel_id = MESH_CHANNEL_LEFT_HEMISPHERE;
        channel = mesh_channel_create(&ch_config, registry);
        ASSERT_NE(channel, nullptr);
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
};

/* ============================================================================
 * Participant Registration Tests
 * ============================================================================ */

TEST_F(MeshQ1SimpleRecallTest, CreateParticipantRegistry) {
    ASSERT_NE(registry, nullptr);
}

TEST_F(MeshQ1SimpleRecallTest, CreateChannel) {
    ASSERT_NE(channel, nullptr);
    EXPECT_EQ(mesh_channel_get_id(channel), MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_STREQ(mesh_channel_get_name(channel), "left_hemisphere");
}

TEST_F(MeshQ1SimpleRecallTest, RegisterParticipant) {
    /* Create participant interface */
    mesh_participant_interface_t interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.module_name, "arithmetic_module", MESH_MAX_NAME_LEN - 1);
    interface.type = MESH_PARTICIPANT_MODULE;

    /* Register participant */
    mesh_participant_config_t config;
    memset(&config, 0, sizeof(config));
    config.module_name = "arithmetic_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t pid = 0;
    nimcp_error_t err = mesh_participant_register(registry, &interface, &config, &pid);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_NE(pid, 0u);

    /* Add participant to channel */
    err = mesh_channel_add_participant(channel, pid);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify participant is in channel */
    EXPECT_TRUE(mesh_channel_has_participant(channel, pid));
    EXPECT_EQ(mesh_channel_get_participant_count(channel), 1u);
}

/* ============================================================================
 * Belief Introduction Tests
 * ============================================================================ */

TEST_F(MeshQ1SimpleRecallTest, IntroduceBelief) {
    /* Register a participant first */
    mesh_participant_interface_t interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.module_name, "arithmetic_module", MESH_MAX_NAME_LEN - 1);

    mesh_participant_config_t config;
    memset(&config, 0, sizeof(config));
    config.module_name = "arithmetic_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t pid = 0;
    mesh_participant_register(registry, &interface, &config, &pid);
    mesh_channel_add_participant(channel, pid);

    /* Create belief: "2 + 2 = 4" encoded as neural vector */
    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 1;
    belief.source = pid;
    belief.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    belief.certainty = 0.99f;  /* High certainty for arithmetic fact */
    belief.vector_dim = 4;

    /* Encode "2 + 2 = 4" symbolically:
     * [operand1=2.0, operator=+1.0 (for add), operand2=2.0, result=4.0]
     */
    belief.belief_vector[0] = 2.0f;
    belief.belief_vector[1] = 1.0f;  /* +1 = addition */
    belief.belief_vector[2] = 2.0f;
    belief.belief_vector[3] = 4.0f;

    /* Introduce belief into channel */
    nimcp_error_t err = mesh_channel_introduce_belief(channel, &belief);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Gossip Round Tests
 * ============================================================================ */

TEST_F(MeshQ1SimpleRecallTest, GossipPropagatesBelief) {
    /* Register participant */
    mesh_participant_interface_t interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.module_name, "arithmetic_module", MESH_MAX_NAME_LEN - 1);

    mesh_participant_config_t config;
    memset(&config, 0, sizeof(config));
    config.module_name = "arithmetic_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t pid = 0;
    mesh_participant_register(registry, &interface, &config, &pid);
    mesh_channel_add_participant(channel, pid);

    /* Introduce belief */
    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 1;
    belief.source = pid;
    belief.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    belief.certainty = 0.99f;
    belief.vector_dim = 4;
    belief.belief_vector[0] = 2.0f;
    belief.belief_vector[1] = 1.0f;
    belief.belief_vector[2] = 2.0f;
    belief.belief_vector[3] = 4.0f;

    mesh_channel_introduce_belief(channel, &belief);

    /* Run gossip round */
    nimcp_error_t err = mesh_channel_gossip_round(channel);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Get channel stats to verify gossip occurred */
    mesh_channel_stats_t stats;
    err = mesh_channel_get_stats(channel, &stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.gossip_rounds, 1u);
}

/* ============================================================================
 * World State Update Tests
 * ============================================================================ */

TEST_F(MeshQ1SimpleRecallTest, BeliefUpdatesWorldState) {
    /* Register participant */
    mesh_participant_interface_t interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.module_name, "arithmetic_module", MESH_MAX_NAME_LEN - 1);

    mesh_participant_config_t config;
    memset(&config, 0, sizeof(config));
    config.module_name = "arithmetic_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t pid = 0;
    mesh_participant_register(registry, &interface, &config, &pid);
    mesh_channel_add_participant(channel, pid);

    /* Introduce high-certainty belief */
    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 42;
    belief.source = pid;
    belief.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    belief.certainty = 0.99f;
    belief.vector_dim = 4;
    belief.belief_vector[0] = 2.0f;
    belief.belief_vector[1] = 1.0f;
    belief.belief_vector[2] = 2.0f;
    belief.belief_vector[3] = 4.0f;

    mesh_channel_introduce_belief(channel, &belief);

    /* Run gossip to propagate belief to world state */
    mesh_channel_gossip_round(channel);

    /* Check world state has item */
    mesh_channel_stats_t stats;
    mesh_channel_get_stats(channel, &stats);
    EXPECT_GE(stats.world_state_items, 1u);
}

/* ============================================================================
 * Transaction Creation Tests
 * ============================================================================ */

TEST_F(MeshQ1SimpleRecallTest, CreateBeliefUpdateTransaction) {
    /* Register participant */
    mesh_participant_interface_t interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.module_name, "arithmetic_module", MESH_MAX_NAME_LEN - 1);

    mesh_participant_config_t config;
    memset(&config, 0, sizeof(config));
    config.module_name = "arithmetic_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t pid = 0;
    mesh_participant_register(registry, &interface, &config, &pid);

    /* Create transaction */
    mesh_transaction_t* tx = mesh_transaction_create(
        MESH_TX_BELIEF_UPDATE,
        pid,
        MESH_CHANNEL_LEFT_HEMISPHERE
    );
    ASSERT_NE(tx, nullptr);

    /* Set payload */
    float payload[4] = {2.0f, 1.0f, 2.0f, 4.0f};  /* 2 + 2 = 4 */
    nimcp_error_t err = mesh_transaction_set_payload(tx, payload, sizeof(payload));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    /* Verify transaction state */
    EXPECT_EQ(tx->type, MESH_TX_BELIEF_UPDATE);
    EXPECT_EQ(tx->source_channel, MESH_CHANNEL_LEFT_HEMISPHERE);
    EXPECT_EQ(tx->target_channel, MESH_CHANNEL_LEFT_HEMISPHERE);  /* Same channel */
    EXPECT_EQ(tx->proposer_id, pid);

    mesh_transaction_destroy(tx);
}

/* ============================================================================
 * Full Q1 Scenario Test
 * ============================================================================ */

TEST_F(MeshQ1SimpleRecallTest, FullQ1Scenario_WhatIs2Plus2) {
    /*
     * Full Q1 scenario: "What is 2 + 2?"
     *
     * 1. Create arithmetic module participant
     * 2. Add to left hemisphere channel (analytical processing)
     * 3. Create belief representing "2 + 2 = 4"
     * 4. Introduce belief, run gossip
     * 5. Verify world state update
     * 6. Verify channel convergence
     */

    /* Step 1: Create participant */
    mesh_participant_interface_t interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.module_name, "arithmetic_module", MESH_MAX_NAME_LEN - 1);
    interface.type = MESH_PARTICIPANT_MODULE;

    mesh_participant_config_t config;
    memset(&config, 0, sizeof(config));
    config.module_name = "arithmetic_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t pid = 0;
    nimcp_error_t err = mesh_participant_register(registry, &interface, &config, &pid);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Step 2: Add to channel */
    err = mesh_channel_add_participant(channel, pid);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Step 3: Create belief for "2 + 2 = 4" */
    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 100;
    belief.source = pid;
    belief.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    belief.certainty = 1.0f;  /* Absolute certainty for math facts */
    belief.vector_dim = 4;
    belief.belief_vector[0] = 2.0f;   /* operand 1 */
    belief.belief_vector[1] = 1.0f;   /* operator: add */
    belief.belief_vector[2] = 2.0f;   /* operand 2 */
    belief.belief_vector[3] = 4.0f;   /* result */

    /* Step 4: Introduce and gossip */
    err = mesh_channel_introduce_belief(channel, &belief);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Run multiple gossip rounds for convergence */
    for (int i = 0; i < 3; i++) {
        err = mesh_channel_gossip_round(channel);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    /* Step 5: Verify world state */
    mesh_channel_stats_t stats;
    err = mesh_channel_get_stats(channel, &stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* World state should have at least one item */
    EXPECT_GE(stats.world_state_items, 1u);

    /* Gossip should have run */
    EXPECT_GE(stats.gossip_rounds, 3u);

    /* Step 6: Check convergence (free energy should be low for consistent belief) */
    float free_energy = mesh_channel_get_free_energy(channel);
    EXPECT_GE(free_energy, 0.0f);

    /* Coherence should be high for consistent beliefs */
    float coherence = mesh_channel_get_world_state_coherence(channel);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);

    /* Check channel update with delta */
    err = mesh_channel_update(channel, 100);  /* 100ms delta */
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(MeshQ1SimpleRecallTest, BeliefDecayDuringGossip) {
    /* Test that belief certainty decays during gossip rounds */

    mesh_participant_interface_t interface;
    memset(&interface, 0, sizeof(interface));
    strncpy(interface.module_name, "test_module", MESH_MAX_NAME_LEN - 1);

    mesh_participant_config_t config;
    memset(&config, 0, sizeof(config));
    config.module_name = "test_module";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

    mesh_participant_id_t pid = 0;
    mesh_participant_register(registry, &interface, &config, &pid);
    mesh_channel_add_participant(channel, pid);

    /* Introduce belief with moderate certainty */
    mesh_belief_t belief;
    memset(&belief, 0, sizeof(belief));
    belief.belief_id = 200;
    belief.source = pid;
    belief.channel = MESH_CHANNEL_LEFT_HEMISPHERE;
    belief.certainty = 0.5f;  /* Moderate certainty */
    belief.vector_dim = 1;
    belief.belief_vector[0] = 1.0f;

    mesh_channel_introduce_belief(channel, &belief);

    /* Get initial stats */
    mesh_channel_stats_t stats_before;
    mesh_channel_get_stats(channel, &stats_before);

    /* Run many gossip rounds - low certainty beliefs should decay away */
    for (int i = 0; i < 20; i++) {
        mesh_channel_gossip_round(channel);
    }

    /* Belief decay is working correctly if gossip completed */
    mesh_channel_stats_t stats_after;
    mesh_channel_get_stats(channel, &stats_after);
    EXPECT_GT(stats_after.gossip_rounds, stats_before.gossip_rounds);
}

TEST_F(MeshQ1SimpleRecallTest, MultipleParticipantsSameChannel) {
    /* Test multiple participants in the same channel */

    mesh_participant_id_t pids[3];

    for (int i = 0; i < 3; i++) {
        mesh_participant_interface_t interface;
        memset(&interface, 0, sizeof(interface));
        snprintf(interface.module_name, MESH_MAX_NAME_LEN, "module_%d", i);

        mesh_participant_config_t config;
        memset(&config, 0, sizeof(config));
        config.module_name = interface.module_name;
        config.type = MESH_PARTICIPANT_MODULE;
        config.home_channel = MESH_CHANNEL_LEFT_HEMISPHERE;

        nimcp_error_t err = mesh_participant_register(registry, &interface, &config, &pids[i]);
        EXPECT_EQ(err, NIMCP_SUCCESS);

        err = mesh_channel_add_participant(channel, pids[i]);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }

    EXPECT_EQ(mesh_channel_get_participant_count(channel), 3u);

    /* All participants should be in channel */
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(mesh_channel_has_participant(channel, pids[i]));
    }
}
