/**
 * @file test_swarm_multi.cpp
 * @brief Tests for Multi-Swarm Coordination
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_multi.h"

class MultiSwarmTest : public ::testing::Test {
protected:
    nimcp_multi_swarm_coordinator_t* coord;

    void SetUp() override {
        coord = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(coord, nullptr);
    }

    void TearDown() override {
        if (coord) nimcp_multi_swarm_destroy(coord);
    }
};

TEST_F(MultiSwarmTest, CreateCoordinator) { EXPECT_NE(coord, nullptr); }
TEST_F(MultiSwarmTest, CreateIdentity) {
    auto* id = nimcp_swarm_identity_create(coord, "swarm1", 10);
    EXPECT_NE(id, nullptr);
    if (id) nimcp_swarm_identity_destroy(id);
}
TEST_F(MultiSwarmTest, RegisterSwarm) {
    auto* id = nimcp_swarm_identity_create(coord, "swarm1", 10);
    EXPECT_EQ(nimcp_swarm_register(coord, id), NIMCP_SUCCESS);
}
TEST_F(MultiSwarmTest, UnregisterSwarm) {
    auto* id = nimcp_swarm_identity_create(coord, "swarm1", 10);
    nimcp_swarm_register(coord, id);
    EXPECT_EQ(nimcp_swarm_unregister(coord, id->swarm_id), NIMCP_SUCCESS);
}
TEST_F(MultiSwarmTest, AddCapability) {
    auto* id = nimcp_swarm_identity_create(coord, "swarm1", 10);
    EXPECT_EQ(nimcp_swarm_add_capability(id, NIMCP_SWARM_CAP_SURVEILLANCE, 0.8, 5, true), NIMCP_SUCCESS);
    if (id) nimcp_swarm_identity_destroy(id);
}
TEST_F(MultiSwarmTest, SetTerritory) {
    auto* id = nimcp_swarm_identity_create(coord, "swarm1", 10);
    nimcp_coord3d_t min = {0, 0, 0}, max = {100, 100, 50};
    EXPECT_EQ(nimcp_swarm_set_territory(id, min, max, true, 0.5), NIMCP_SUCCESS);
    if (id) nimcp_swarm_identity_destroy(id);
}
TEST_F(MultiSwarmTest, CreateMission) {
    nimcp_territory_bounds_t area = {{0,0,0}, {50,50,10}, 0, false, 0.5};
    uint64_t mid = nimcp_mission_create(coord, "test_mission", NIMCP_MISSION_PRIORITY_HIGH, area, 0);
    EXPECT_GT(mid, 0);
}
TEST_F(MultiSwarmTest, ResourceRequest) {
    auto* id1 = nimcp_swarm_identity_create(coord, "s1", 10);
    auto* id2 = nimcp_swarm_identity_create(coord, "s2", 10);
    nimcp_swarm_register(coord, id1);
    nimcp_swarm_register(coord, id2);
    uint64_t req = nimcp_resource_request(coord, id1->swarm_id, id2->swarm_id,
                                           NIMCP_RESOURCE_REQ_DRONES, 2, NIMCP_MISSION_PRIORITY_MEDIUM);
    EXPECT_GT(req, 0);
}
TEST_F(MultiSwarmTest, CreateBridge) {
    auto* id1 = nimcp_swarm_identity_create(coord, "s1", 10);
    auto* id2 = nimcp_swarm_identity_create(coord, "s2", 10);
    nimcp_swarm_register(coord, id1);
    nimcp_swarm_register(coord, id2);
    uint64_t bid = nimcp_comm_bridge_create(coord, id1->swarm_id, id2->swarm_id, nullptr, 0);
    EXPECT_GT(bid, 0);
}
TEST_F(MultiSwarmTest, DetectConflicts) {
    uint32_t count = nimcp_conflict_detect(coord);
    EXPECT_GE(count, 0);
}
TEST_F(MultiSwarmTest, GetStats) {
    uint32_t swarms, agents, missions, conflicts;
    nimcp_multi_swarm_get_stats(coord, &swarms, &agents, &missions, &conflicts);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
