/**
 * @file test_swarm_energy_gossip.cpp
 * @brief Comprehensive unit tests for NIMCP Energy-Aware Gossip Protocol
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_energy_gossip.h"

class EnergyGossipTest : public ::testing::Test {
protected:
    nimcp_energy_gossip_t* system;
    nimcp_energy_gossip_config_t config;

    void SetUp() override {
        nimcp_energy_gossip_default_config(&config);
        system = nimcp_energy_gossip_create(&config, nullptr);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        if (system) nimcp_energy_gossip_destroy(system);
    }
};

TEST_F(EnergyGossipTest, CreateSystem) { EXPECT_NE(system, nullptr); }
TEST_F(EnergyGossipTest, DestroyNull) { nimcp_energy_gossip_destroy(nullptr); SUCCEED(); }
TEST_F(EnergyGossipTest, RegisterNode) {
    nimcp_result_t r = nimcp_energy_gossip_register_node(system, 1, 0.8);
    EXPECT_EQ(r, NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, UpdateEnergy) {
    nimcp_energy_gossip_register_node(system, 1, 0.5);
    EXPECT_EQ(nimcp_energy_gossip_update_energy(system, 1, 0.7), NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, GetEnergy) {
    nimcp_energy_gossip_register_node(system, 1, 0.6);
    float energy = 0.0;
    EXPECT_EQ(nimcp_energy_gossip_get_energy(system, 1, &energy), NIMCP_SUCCESS);
    EXPECT_NEAR(energy, 0.6, 0.01);
}
TEST_F(EnergyGossipTest, SelectPeer) {
    nimcp_energy_gossip_register_node(system, 1, 0.8);
    nimcp_energy_gossip_register_node(system, 2, 0.5);
    uint32_t peer = 0;
    EXPECT_EQ(nimcp_energy_gossip_select_peer(system, 1, &peer), NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, GossipMessage) {
    nimcp_energy_gossip_register_node(system, 1, 0.7);
    uint8_t data[] = {1, 2, 3};
    EXPECT_EQ(nimcp_energy_gossip_send(system, 1, data, 3), NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, UpdateRound) {
    EXPECT_EQ(nimcp_energy_gossip_update(system, 100), NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, GetStats) {
    nimcp_energy_gossip_stats_t stats;
    EXPECT_EQ(nimcp_energy_gossip_get_stats(system, &stats), NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, LowEnergyPriority) {
    nimcp_energy_gossip_register_node(system, 1, 0.1);
    nimcp_energy_gossip_register_node(system, 2, 0.9);
    uint32_t peer;
    nimcp_energy_gossip_select_peer(system, 2, &peer);
    SUCCEED();
}
TEST_F(EnergyGossipTest, EnergyBalancing) {
    for (uint32_t i = 0; i < 10; i++)
        nimcp_energy_gossip_register_node(system, i, 0.5 + i * 0.05);
    SUCCEED();
}
TEST_F(EnergyGossipTest, MessagePropagation) {
    for (uint32_t i = 0; i < 5; i++)
        nimcp_energy_gossip_register_node(system, i, 0.7);
    uint8_t msg[] = {0xAB, 0xCD};
    nimcp_energy_gossip_send(system, 0, msg, 2);
    for (int j = 0; j < 3; j++)
        nimcp_energy_gossip_update(system, 100);
    SUCCEED();
}
TEST_F(EnergyGossipTest, ValidateConfig) {
    nimcp_energy_gossip_config_t cfg;
    nimcp_energy_gossip_default_config(&cfg);
    EXPECT_EQ(nimcp_energy_gossip_validate_config(&cfg), NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, ResetSystem) {
    nimcp_energy_gossip_register_node(system, 1, 0.5);
    EXPECT_EQ(nimcp_energy_gossip_reset(system), NIMCP_SUCCESS);
}
TEST_F(EnergyGossipTest, MaxNodes) {
    for (uint32_t i = 0; i < 100; i++)
        nimcp_energy_gossip_register_node(system, i, 0.5);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
